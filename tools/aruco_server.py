"""
ArUco Pose Verification Server
==============================

This server receives camera frames from Pico W over TCP,
detects ArUco markers, estimates 3D pose, and verifies
if the pose is within tolerance for 2 consecutive seconds.

Architecture:
    Pico W + OV7670  ---(TCP)---> This Server ---(Response)---> Pico W
         |                              |
         |                              v
         |                      OpenCV ArUco Detection
         |                      cv2.aruco.detectMarkers()
         |                      cv2.aruco.estimatePoseSingleMarkers()
         |                              |
         v                              v
    Unlock Signal <-------- Pose Verification (2 seconds)

Requirements:
    pip install opencv-python opencv-contrib-python numpy

Usage:
    python aruco_server.py [--port 8888] [--marker-id 42] [--debug]

Author: MicroDrive Team
Date: November 2025
"""

import socket
import struct
import argparse
import time
import threading
import sys
from dataclasses import dataclass
from typing import Optional, Tuple
import numpy as np

try:
    import cv2
    from cv2 import aruco
except ImportError:
    print("ERROR: OpenCV not found. Install with:")
    print("  pip install opencv-python opencv-contrib-python")
    sys.exit(1)


# =============================================================================
# Configuration
# =============================================================================

@dataclass
class Config:
    """Server configuration"""
    port: int = 8888
    target_marker_id: int = 42  # Expected ArUco marker ID
    marker_size: float = 0.05   # Marker size in meters (5cm)
    
    # Pose tolerances (tight multi-axis tolerances as required)
    pos_tolerance: Tuple[float, float, float] = (0.02, 0.02, 0.05)  # X, Y, Z in meters
    rot_tolerance: Tuple[float, float, float] = (10.0, 10.0, 15.0)  # Rx, Ry, Rz in degrees
    
    # Target pose (where the marker should be)
    target_pos: Tuple[float, float, float] = (0.0, 0.0, 0.30)  # 30cm in front of camera
    target_rot: Tuple[float, float, float] = (0.0, 0.0, 0.0)   # Facing camera
    
    # Verification timing
    verification_time_ms: int = 2000  # Must hold pose for 2 seconds
    
    # Debug/display options
    debug: bool = False
    show_video: bool = True


# =============================================================================
# Frame Protocol
# =============================================================================

# Frame header format (must match Pico's frame_header_t)
FRAME_HEADER_FORMAT = '<IIHHHHI'  # magic(4), frame_id(4), w(2), h(2), fmt(2), size(2), checksum(4)
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER_FORMAT)
FRAME_MAGIC = 0xCAFEBABE

# Response format (must match Pico's pose_response_t)
RESPONSE_FORMAT = '<IBBBBffffff'  # magic(4), found(1), id(1), valid(1), unlock(1), pos(3*4), rot(3*4)
RESPONSE_MAGIC = 0xDEADBEEF


@dataclass
class PoseResult:
    """Pose detection result"""
    marker_found: bool = False
    marker_id: int = -1
    pose_valid: bool = False
    unlock_ready: bool = False
    position: Tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation: Tuple[float, float, float] = (0.0, 0.0, 0.0)


# =============================================================================
# Camera Calibration
# =============================================================================

class CameraCalibration:
    """Camera intrinsic parameters for OV7670 320x240"""
    
    def __init__(self, width: int = 320, height: int = 240):
        # Approximate camera matrix for OV7670
        # These values should be calibrated for your specific camera!
        focal_length = width * 0.8  # Approximate focal length
        
        self.camera_matrix = np.array([
            [focal_length, 0, width / 2],
            [0, focal_length, height / 2],
            [0, 0, 1]
        ], dtype=np.float32)
        
        # Distortion coefficients (assume minimal distortion)
        self.dist_coeffs = np.zeros((5, 1), dtype=np.float32)
        
    @classmethod
    def from_calibration_file(cls, filepath: str) -> 'CameraCalibration':
        """Load calibration from file (if available)"""
        cal = cls()
        try:
            data = np.load(filepath)
            cal.camera_matrix = data['camera_matrix']
            cal.dist_coeffs = data['dist_coeffs']
            print(f"[Calibration] Loaded from {filepath}")
        except:
            print(f"[Calibration] Using default values (consider calibrating!)")
        return cal


# =============================================================================
# ArUco Detector
# =============================================================================

class ArucoDetector:
    """ArUco marker detection and pose estimation"""
    
    def __init__(self, config: Config, calibration: CameraCalibration):
        self.config = config
        self.calibration = calibration
        
        # ArUco dictionary (use 4x4_50 for simplicity)
        self.aruco_dict = aruco.getPredefinedDictionary(aruco.DICT_4X4_50)
        self.aruco_params = aruco.DetectorParameters()
        
        # Optimize parameters for low resolution
        self.aruco_params.adaptiveThreshWinSizeMin = 3
        self.aruco_params.adaptiveThreshWinSizeMax = 23
        self.aruco_params.adaptiveThreshWinSizeStep = 10
        self.aruco_params.minMarkerPerimeterRate = 0.03
        
        # Pose verification state
        self.pose_valid_since: Optional[float] = None
        self.last_valid_pose: Optional[PoseResult] = None
        
    def detect_and_estimate(self, frame: np.ndarray) -> PoseResult:
        """
        Detect ArUco marker and estimate 3D pose
        
        Args:
            frame: BGR image (converted from YUV422)
            
        Returns:
            PoseResult with detection and pose information
        """
        result = PoseResult()
        
        # Convert to grayscale for detection
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Detect markers
        corners, ids, rejected = aruco.detectMarkers(
            gray, 
            self.aruco_dict, 
            parameters=self.aruco_params
        )
        
        if ids is None or len(ids) == 0:
            self._reset_pose_timer()
            return result
        
        # Look for target marker
        target_idx = None
        for i, marker_id in enumerate(ids.flatten()):
            if marker_id == self.config.target_marker_id:
                target_idx = i
                break
        
        if target_idx is None:
            # Found markers but not the target
            result.marker_found = True
            result.marker_id = int(ids[0][0])
            self._reset_pose_timer()
            return result
        
        # Found target marker
        result.marker_found = True
        result.marker_id = self.config.target_marker_id
        
        # Estimate pose
        rvecs, tvecs, _ = aruco.estimatePoseSingleMarkers(
            [corners[target_idx]],
            self.config.marker_size,
            self.calibration.camera_matrix,
            self.calibration.dist_coeffs
        )
        
        # Extract pose
        tvec = tvecs[0][0]  # Translation vector (X, Y, Z)
        rvec = rvecs[0][0]  # Rotation vector (Rodrigues)
        
        # Convert rotation to Euler angles (degrees)
        rotation_matrix, _ = cv2.Rodrigues(rvec)
        euler = self._rotation_matrix_to_euler(rotation_matrix)
        
        result.position = (float(tvec[0]), float(tvec[1]), float(tvec[2]))
        result.rotation = (
            float(np.degrees(euler[0])),
            float(np.degrees(euler[1])),
            float(np.degrees(euler[2]))
        )
        
        # Check pose validity
        result.pose_valid = self._check_pose_tolerance(result)
        
        # Check unlock timing
        if result.pose_valid:
            if self.pose_valid_since is None:
                self.pose_valid_since = time.time()
            
            elapsed_ms = (time.time() - self.pose_valid_since) * 1000
            if elapsed_ms >= self.config.verification_time_ms:
                result.unlock_ready = True
        else:
            self._reset_pose_timer()
        
        self.last_valid_pose = result
        return result
    
    def _check_pose_tolerance(self, result: PoseResult) -> bool:
        """Check if pose is within configured tolerance"""
        
        # Check position tolerance
        pos_diff = (
            abs(result.position[0] - self.config.target_pos[0]),
            abs(result.position[1] - self.config.target_pos[1]),
            abs(result.position[2] - self.config.target_pos[2])
        )
        
        if (pos_diff[0] > self.config.pos_tolerance[0] or
            pos_diff[1] > self.config.pos_tolerance[1] or
            pos_diff[2] > self.config.pos_tolerance[2]):
            return False
        
        # Check rotation tolerance
        rot_diff = (
            abs(result.rotation[0] - self.config.target_rot[0]),
            abs(result.rotation[1] - self.config.target_rot[1]),
            abs(result.rotation[2] - self.config.target_rot[2])
        )
        
        if (rot_diff[0] > self.config.rot_tolerance[0] or
            rot_diff[1] > self.config.rot_tolerance[1] or
            rot_diff[2] > self.config.rot_tolerance[2]):
            return False
        
        return True
    
    def _reset_pose_timer(self):
        """Reset the pose verification timer"""
        self.pose_valid_since = None
    
    def _rotation_matrix_to_euler(self, R: np.ndarray) -> Tuple[float, float, float]:
        """Convert rotation matrix to Euler angles (XYZ convention)"""
        sy = np.sqrt(R[0, 0] ** 2 + R[1, 0] ** 2)
        
        singular = sy < 1e-6
        
        if not singular:
            x = np.arctan2(R[2, 1], R[2, 2])
            y = np.arctan2(-R[2, 0], sy)
            z = np.arctan2(R[1, 0], R[0, 0])
        else:
            x = np.arctan2(-R[1, 2], R[1, 1])
            y = np.arctan2(-R[2, 0], sy)
            z = 0
        
        return (x, y, z)
    
    def get_time_remaining(self) -> float:
        """Get time remaining for unlock (ms), or -1 if not tracking"""
        if self.pose_valid_since is None:
            return -1
        
        elapsed_ms = (time.time() - self.pose_valid_since) * 1000
        remaining = self.config.verification_time_ms - elapsed_ms
        return max(0, remaining)
    
    def draw_detection(self, frame: np.ndarray, result: PoseResult) -> np.ndarray:
        """Draw detection visualization on frame"""
        
        # Draw marker outline if detected
        if result.marker_found and self.last_valid_pose:
            # Draw pose axis
            rvec = cv2.Rodrigues(np.array([
                [np.radians(result.rotation[0])],
                [np.radians(result.rotation[1])],
                [np.radians(result.rotation[2])]
            ]))[0]
            tvec = np.array(result.position).reshape(3, 1)
            
            # Draw coordinate axes
            cv2.drawFrameAxes(
                frame,
                self.calibration.camera_matrix,
                self.calibration.dist_coeffs,
                rvec, tvec,
                self.config.marker_size * 0.5
            )
        
        # Status text
        status_color = (0, 255, 0) if result.pose_valid else (0, 0, 255)
        
        cv2.putText(frame, f"Marker: {'Found' if result.marker_found else 'None'}",
                    (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, status_color, 1)
        
        if result.marker_found:
            cv2.putText(frame, f"ID: {result.marker_id}",
                        (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.5, status_color, 1)
            
            cv2.putText(frame, f"Pos: ({result.position[0]:.2f}, {result.position[1]:.2f}, {result.position[2]:.2f})",
                        (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.5, status_color, 1)
            
            cv2.putText(frame, f"Rot: ({result.rotation[0]:.1f}, {result.rotation[1]:.1f}, {result.rotation[2]:.1f})",
                        (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.5, status_color, 1)
            
            remaining = self.get_time_remaining()
            if remaining >= 0:
                cv2.putText(frame, f"Unlock in: {remaining/1000:.1f}s",
                            (10, 100), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
        
        if result.unlock_ready:
            cv2.putText(frame, "*** UNLOCK ***",
                        (frame.shape[1]//2 - 60, frame.shape[0]//2),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        
        return frame


# =============================================================================
# Image Conversion
# =============================================================================

def yuv422_to_bgr(yuv_data: bytes, width: int, height: int) -> np.ndarray:
    """
    Convert YUV422 (YUYV) to BGR
    
    YUV422 format: Y0 U0 Y1 V0 (4 bytes for 2 pixels)
    """
    # Reshape to YUV422 format
    yuv = np.frombuffer(yuv_data, dtype=np.uint8)
    yuv = yuv.reshape((height, width, 2))
    
    # Convert YUV422 to BGR
    bgr = cv2.cvtColor(yuv, cv2.COLOR_YUV2BGR_YUYV)
    
    return bgr


# =============================================================================
# TCP Server
# =============================================================================

class ArucoServer:
    """TCP server for receiving frames and sending pose responses"""
    
    def __init__(self, config: Config):
        self.config = config
        self.calibration = CameraCalibration()
        self.detector = ArucoDetector(config, self.calibration)
        
        self.running = False
        self.client_socket = None
        
        # Statistics
        self.frame_count = 0
        self.start_time = time.time()
        
    def start(self):
        """Start the TCP server"""
        
        # Create server socket
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind(('0.0.0.0', self.config.port))
        self.server_socket.listen(1)
        
        print(f"\n{'='*60}")
        print(f"  ArUco Pose Verification Server")
        print(f"{'='*60}")
        print(f"  Port: {self.config.port}")
        print(f"  Target Marker ID: {self.config.target_marker_id}")
        print(f"  Verification Time: {self.config.verification_time_ms}ms")
        print(f"  Position Tolerance: {self.config.pos_tolerance}")
        print(f"  Rotation Tolerance: {self.config.rot_tolerance}°")
        print(f"{'='*60}")
        print(f"\n  Waiting for Pico W connection...\n")
        
        self.running = True
        
        while self.running:
            try:
                # Accept connection
                self.client_socket, addr = self.server_socket.accept()
                print(f"[Server] Client connected from {addr}")
                
                # Handle client
                self._handle_client()
                
            except Exception as e:
                print(f"[Server] Error: {e}")
                
        self.server_socket.close()
    
    def _handle_client(self):
        """Handle connected client"""
        
        self.frame_count = 0
        self.start_time = time.time()
        
        while self.running and self.client_socket:
            try:
                # Receive frame header
                header_data = self._recv_exact(FRAME_HEADER_SIZE)
                if not header_data:
                    break
                
                # Parse header
                magic, frame_id, width, height, fmt, data_size, checksum = \
                    struct.unpack(FRAME_HEADER_FORMAT, header_data)
                
                if magic != FRAME_MAGIC:
                    print(f"[Server] Invalid magic: 0x{magic:08X}")
                    continue
                
                # Receive frame data
                # Note: data_size is uint16, so max is 65535
                # For 320x240x2 = 153600, we need to read more
                expected_size = width * height * 2  # YUV422
                frame_data = self._recv_exact(expected_size)
                if not frame_data:
                    break
                
                # Convert to BGR
                frame = yuv422_to_bgr(frame_data, width, height)
                
                # Detect and estimate pose
                result = self.detector.detect_and_estimate(frame)
                
                # Send response
                self._send_response(result)
                
                # Display if enabled
                if self.config.show_video:
                    display_frame = self.detector.draw_detection(frame.copy(), result)
                    cv2.imshow("ArUco Detection", display_frame)
                    key = cv2.waitKey(1)
                    if key == ord('q'):
                        self.running = False
                
                # Statistics
                self.frame_count += 1
                if self.frame_count % 30 == 0:
                    elapsed = time.time() - self.start_time
                    fps = self.frame_count / elapsed
                    print(f"[Server] Processed {self.frame_count} frames ({fps:.1f} FPS)")
                
            except Exception as e:
                print(f"[Server] Frame processing error: {e}")
                break
        
        print("[Server] Client disconnected")
        if self.client_socket:
            self.client_socket.close()
            self.client_socket = None
    
    def _recv_exact(self, num_bytes: int) -> Optional[bytes]:
        """Receive exact number of bytes"""
        data = b''
        while len(data) < num_bytes:
            try:
                chunk = self.client_socket.recv(num_bytes - len(data))
                if not chunk:
                    return None
                data += chunk
            except:
                return None
        return data
    
    def _send_response(self, result: PoseResult):
        """Send pose response to client"""
        response = struct.pack(
            RESPONSE_FORMAT,
            RESPONSE_MAGIC,
            1 if result.marker_found else 0,
            result.marker_id if result.marker_id >= 0 else 255,
            1 if result.pose_valid else 0,
            1 if result.unlock_ready else 0,
            result.position[0], result.position[1], result.position[2],
            result.rotation[0], result.rotation[1], result.rotation[2]
        )
        
        try:
            self.client_socket.send(response)
        except:
            pass
    
    def stop(self):
        """Stop the server"""
        self.running = False
        if self.client_socket:
            self.client_socket.close()


# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='ArUco Pose Verification Server for MicroDrive'
    )
    parser.add_argument('--port', type=int, default=8888,
                        help='TCP port to listen on (default: 8888)')
    parser.add_argument('--marker-id', type=int, default=42,
                        help='Target ArUco marker ID (default: 42)')
    parser.add_argument('--marker-size', type=float, default=0.05,
                        help='Marker size in meters (default: 0.05)')
    parser.add_argument('--no-display', action='store_true',
                        help='Disable video display window')
    parser.add_argument('--debug', action='store_true',
                        help='Enable debug output')
    
    # Tolerance options
    parser.add_argument('--pos-tol', type=float, nargs=3, default=[0.02, 0.02, 0.05],
                        help='Position tolerance X Y Z in meters')
    parser.add_argument('--rot-tol', type=float, nargs=3, default=[10.0, 10.0, 15.0],
                        help='Rotation tolerance Rx Ry Rz in degrees')
    
    # Target pose
    parser.add_argument('--target-pos', type=float, nargs=3, default=[0.0, 0.0, 0.30],
                        help='Target position X Y Z in meters')
    parser.add_argument('--target-rot', type=float, nargs=3, default=[0.0, 0.0, 0.0],
                        help='Target rotation Rx Ry Rz in degrees')
    
    args = parser.parse_args()
    
    # Create configuration
    config = Config(
        port=args.port,
        target_marker_id=args.marker_id,
        marker_size=args.marker_size,
        pos_tolerance=tuple(args.pos_tol),
        rot_tolerance=tuple(args.rot_tol),
        target_pos=tuple(args.target_pos),
        target_rot=tuple(args.target_rot),
        debug=args.debug,
        show_video=not args.no_display
    )
    
    # Start server
    server = ArucoServer(config)
    
    try:
        server.start()
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
    finally:
        server.stop()
        cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
