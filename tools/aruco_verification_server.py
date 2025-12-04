#!/usr/bin/env python3
"""
ArUco Marker Verification Server
==================================
Receives camera frames from Pico W, detects ArUco markers,
verifies position/rotation/scale within tight tolerances for 2 consecutive sequences.
Logs all requests, responses, and received images.

Author: MicroDrive Team
Date: December 4, 2025
"""

import socket
import struct
import cv2
import numpy as np
from datetime import datetime
import json
import os
import sys
import threading
import time
from pathlib import Path
import argparse

# ============================================================================
# Configuration
# ============================================================================

class Config:
    """Server configuration with tolerance thresholds"""
    
    # Server settings
    HOST = '0.0.0.0'  # Listen on all interfaces
    PORT = 8888
    
    # ArUco settings
    ARUCO_DICT = cv2.aruco.DICT_4X4_50
    TARGET_MARKER_ID = 0  # The marker ID we're looking for
    
    # Camera calibration (modify based on your camera)
    CAMERA_MATRIX = np.array([
        [500, 0, 160],
        [0, 500, 120],
        [0, 0, 1]
    ], dtype=np.float32)
    
    DIST_COEFFS = np.zeros((5, 1))  # No distortion for initial testing
    MARKER_SIZE = 0.05  # Marker size in meters (5cm)
    
    # Multi-axis tolerance thresholds (tight tolerances)
    TOLERANCE_POSITION_X_MM = 5.0    # ±5mm in X
    TOLERANCE_POSITION_Y_MM = 5.0    # ±5mm in Y
    TOLERANCE_POSITION_Z_MM = 10.0   # ±10mm in Z (depth)
    TOLERANCE_ROTATION_DEG = 3.0     # ±3 degrees for each axis
    TOLERANCE_SCALE_PERCENT = 5.0    # ±5% scale variation
    
    # Target pose (reference position)
    TARGET_POS_X_MM = 0.0
    TARGET_POS_Y_MM = 0.0
    TARGET_POS_Z_MM = 200.0  # 20cm from camera
    TARGET_ROT_X_DEG = 0.0
    TARGET_ROT_Y_DEG = 0.0
    TARGET_ROT_Z_DEG = 0.0
    
    # Consecutive verification requirements
    CONSECUTIVE_FRAMES_REQUIRED = 2
    VERIFICATION_WINDOW_SECONDS = 3.0  # Must happen within 3 seconds
    
    # Logging
    LOG_DIR = 'aruco_logs'
    SAVE_ALL_IMAGES = True
    SAVE_FAILED_IMAGES = True
    VERBOSE = True


# ============================================================================
# Data Structures
# ============================================================================

class FrameHeader:
    """Frame header structure matching Pico W"""
    MAGIC = 0xCAFEBABE
    FORMAT = '<IIHHHHI'  # magic, frame_id, w, h, format, data_size, checksum
    SIZE = struct.calcsize(FORMAT)
    
    def __init__(self, data):
        unpacked = struct.unpack(self.FORMAT, data)
        self.magic = unpacked[0]
        self.frame_id = unpacked[1]
        self.width = unpacked[2]
        self.height = unpacked[3]
        self.format = unpacked[4]
        self.data_size = unpacked[5]
        self.checksum = unpacked[6]
    
    def is_valid(self):
        return self.magic == self.MAGIC


class PoseResponse:
    """Response structure to send back to Pico W"""
    MAGIC = 0xDEADBEEF
    FORMAT = '<IBBBBffffff'  # magic, marker_found, marker_id, pose_valid, unlock_ready, pos_xyz, rot_xyz
    SIZE = struct.calcsize(FORMAT)
    
    def __init__(self):
        self.marker_found = 0
        self.marker_id = 0
        self.pose_valid = 0
        self.unlock_ready = 0
        self.pos_x = 0.0
        self.pos_y = 0.0
        self.pos_z = 0.0
        self.rot_x = 0.0
        self.rot_y = 0.0
        self.rot_z = 0.0
    
    def pack(self):
        return struct.pack(
            self.FORMAT,
            self.MAGIC,
            self.marker_found,
            self.marker_id,
            self.pose_valid,
            self.unlock_ready,
            self.pos_x,
            self.pos_y,
            self.pos_z,
            self.rot_x,
            self.rot_y,
            self.rot_z
        )


class VerificationState:
    """Tracks consecutive valid frames for verification"""
    
    def __init__(self):
        self.valid_frames = []  # List of (timestamp, pose) tuples
        self.last_verification_time = None
        self.total_verifications = 0
        self.lock = threading.Lock()
    
    def add_valid_frame(self, pose_data):
        """Add a valid frame and check if verification complete"""
        with self.lock:
            timestamp = time.time()
            self.valid_frames.append((timestamp, pose_data))
            
            # Remove frames outside the time window
            cutoff_time = timestamp - Config.VERIFICATION_WINDOW_SECONDS
            self.valid_frames = [
                (t, p) for t, p in self.valid_frames if t >= cutoff_time
            ]
            
            # Check if we have enough consecutive frames
            if len(self.valid_frames) >= Config.CONSECUTIVE_FRAMES_REQUIRED:
                self.last_verification_time = timestamp
                self.total_verifications += 1
                return True
            
            return False
    
    def reset(self):
        """Reset verification state"""
        with self.lock:
            self.valid_frames = []
    
    def is_verified(self):
        """Check if currently verified"""
        with self.lock:
            return len(self.valid_frames) >= Config.CONSECUTIVE_FRAMES_REQUIRED


# ============================================================================
# ArUco Detection and Verification
# ============================================================================

class ArucoVerifier:
    """ArUco marker detection and pose verification"""
    
    def __init__(self, config):
        self.config = config
        self.aruco_dict = cv2.aruco.getPredefinedDictionary(config.ARUCO_DICT)
        self.aruco_params = cv2.aruco.DetectorParameters()
        self.detector = cv2.aruco.ArucoDetector(self.aruco_dict, self.aruco_params)
        
        print(f"[ArUco] Initialized detector for marker ID {config.TARGET_MARKER_ID}")
        print(f"[ArUco] Tolerance: Position ±({config.TOLERANCE_POSITION_X_MM}, "
              f"{config.TOLERANCE_POSITION_Y_MM}, {config.TOLERANCE_POSITION_Z_MM})mm")
        print(f"[ArUco] Tolerance: Rotation ±{config.TOLERANCE_ROTATION_DEG}°")
        print(f"[ArUco] Tolerance: Scale ±{config.TOLERANCE_SCALE_PERCENT}%")
        print(f"[ArUco] Verification: {config.CONSECUTIVE_FRAMES_REQUIRED} frames "
              f"within {config.VERIFICATION_WINDOW_SECONDS}s")
    
    def detect_and_verify(self, image):
        """
        Detect ArUco marker and verify pose within tolerances
        
        Returns:
            tuple: (marker_found, marker_id, pose_valid, pose_data, annotated_image)
        """
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        
        # Detect markers
        corners, ids, rejected = self.detector.detectMarkers(gray)
        
        # Annotate image
        annotated = image.copy()
        
        if ids is None or len(ids) == 0:
            return False, 0, False, None, annotated
        
        # Draw detected markers
        cv2.aruco.drawDetectedMarkers(annotated, corners, ids)
        
        # Find target marker
        target_idx = None
        for idx, marker_id in enumerate(ids.flatten()):
            if marker_id == self.config.TARGET_MARKER_ID:
                target_idx = idx
                break
        
        if target_idx is None:
            return True, ids[0][0], False, None, annotated
        
        # Estimate pose
        rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(
            corners,
            self.config.MARKER_SIZE,
            self.config.CAMERA_MATRIX,
            self.config.DIST_COEFFS
        )
        
        # Get pose for target marker
        rvec = rvecs[target_idx][0]
        tvec = tvecs[target_idx][0]
        
        # Draw axis
        cv2.drawFrameAxes(
            annotated,
            self.config.CAMERA_MATRIX,
            self.config.DIST_COEFFS,
            rvec,
            tvec,
            self.config.MARKER_SIZE * 0.5
        )
        
        # Convert to readable format
        pos_x_mm = tvec[0] * 1000
        pos_y_mm = tvec[1] * 1000
        pos_z_mm = tvec[2] * 1000
        
        # Convert rotation vector to Euler angles (in degrees)
        rmat, _ = cv2.Rodrigues(rvec)
        rot_x_deg, rot_y_deg, rot_z_deg = self._rotation_matrix_to_euler(rmat)
        
        # Calculate marker size (scale)
        corner_dist = np.linalg.norm(corners[target_idx][0][0] - corners[target_idx][0][2])
        expected_size = 100  # Expected pixel size at target distance
        scale_percent = (corner_dist / expected_size) * 100
        
        pose_data = {
            'pos_x_mm': pos_x_mm,
            'pos_y_mm': pos_y_mm,
            'pos_z_mm': pos_z_mm,
            'rot_x_deg': rot_x_deg,
            'rot_y_deg': rot_y_deg,
            'rot_z_deg': rot_z_deg,
            'scale_percent': scale_percent
        }
        
        # Check tolerances
        pose_valid = self._check_tolerances(pose_data)
        
        # Add tolerance info to image
        status_color = (0, 255, 0) if pose_valid else (0, 0, 255)
        status_text = "VALID" if pose_valid else "OUT OF TOLERANCE"
        
        cv2.putText(annotated, f"Marker {self.config.TARGET_MARKER_ID}: {status_text}",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, status_color, 2)
        cv2.putText(annotated, f"Pos: ({pos_x_mm:.1f}, {pos_y_mm:.1f}, {pos_z_mm:.1f})mm",
                    (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        cv2.putText(annotated, f"Rot: ({rot_x_deg:.1f}, {rot_y_deg:.1f}, {rot_z_deg:.1f})deg",
                    (10, 85), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        cv2.putText(annotated, f"Scale: {scale_percent:.1f}%",
                    (10, 110), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        
        return True, self.config.TARGET_MARKER_ID, pose_valid, pose_data, annotated
    
    def _check_tolerances(self, pose_data):
        """Check if pose is within all tolerance thresholds"""
        # Position tolerances
        dx = abs(pose_data['pos_x_mm'] - self.config.TARGET_POS_X_MM)
        dy = abs(pose_data['pos_y_mm'] - self.config.TARGET_POS_Y_MM)
        dz = abs(pose_data['pos_z_mm'] - self.config.TARGET_POS_Z_MM)
        
        if dx > self.config.TOLERANCE_POSITION_X_MM:
            return False
        if dy > self.config.TOLERANCE_POSITION_Y_MM:
            return False
        if dz > self.config.TOLERANCE_POSITION_Z_MM:
            return False
        
        # Rotation tolerances
        drx = abs(pose_data['rot_x_deg'] - self.config.TARGET_ROT_X_DEG)
        dry = abs(pose_data['rot_y_deg'] - self.config.TARGET_ROT_Y_DEG)
        drz = abs(pose_data['rot_z_deg'] - self.config.TARGET_ROT_Z_DEG)
        
        if drx > self.config.TOLERANCE_ROTATION_DEG:
            return False
        if dry > self.config.TOLERANCE_ROTATION_DEG:
            return False
        if drz > self.config.TOLERANCE_ROTATION_DEG:
            return False
        
        # Scale tolerance
        dscale = abs(pose_data['scale_percent'] - 100.0)
        if dscale > self.config.TOLERANCE_SCALE_PERCENT:
            return False
        
        return True
    
    def _rotation_matrix_to_euler(self, R):
        """Convert rotation matrix to Euler angles (XYZ convention) in degrees"""
        sy = np.sqrt(R[0, 0] * R[0, 0] + R[1, 0] * R[1, 0])
        singular = sy < 1e-6
        
        if not singular:
            x = np.arctan2(R[2, 1], R[2, 2])
            y = np.arctan2(-R[2, 0], sy)
            z = np.arctan2(R[1, 0], R[0, 0])
        else:
            x = np.arctan2(-R[1, 2], R[1, 1])
            y = np.arctan2(-R[2, 0], sy)
            z = 0
        
        return np.degrees(x), np.degrees(y), np.degrees(z)


# ============================================================================
# Server and Logging
# ============================================================================

class ArucoServer:
    """TCP server for receiving frames and sending verification results"""
    
    def __init__(self, config):
        self.config = config
        self.verifier = ArucoVerifier(config)
        self.verification_state = VerificationState()
        self.setup_logging()
        
        self.total_frames = 0
        self.valid_frames = 0
        self.verified_sequences = 0
        
    def setup_logging(self):
        """Create logging directory structure"""
        self.log_dir = Path(self.config.LOG_DIR)
        self.log_dir.mkdir(exist_ok=True)
        
        # Create subdirectories
        self.images_dir = self.log_dir / 'images'
        self.failed_dir = self.log_dir / 'failed'
        self.images_dir.mkdir(exist_ok=True)
        self.failed_dir.mkdir(exist_ok=True)
        
        # Create log file
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.log_file = self.log_dir / f'session_{timestamp}.log'
        self.json_log_file = self.log_dir / f'session_{timestamp}.json'
        
        self.log_data = []
        
        print(f"[Server] Logging to: {self.log_file}")
        print(f"[Server] Images saved to: {self.images_dir}")
    
    def log_message(self, level, message):
        """Log message to file and console"""
        timestamp = datetime.now().isoformat()
        log_entry = f"[{timestamp}] [{level}] {message}"
        
        with open(self.log_file, 'a') as f:
            f.write(log_entry + '\n')
        
        if self.config.VERBOSE or level == 'ERROR':
            print(log_entry)
    
    def log_transaction(self, frame_id, request_data, response_data):
        """Log complete request/response transaction"""
        transaction = {
            'timestamp': datetime.now().isoformat(),
            'frame_id': frame_id,
            'request': request_data,
            'response': response_data
        }
        
        self.log_data.append(transaction)
        
        # Save JSON log
        with open(self.json_log_file, 'w') as f:
            json.dump(self.log_data, f, indent=2)
    
    def save_image(self, image, frame_id, marker_found, pose_valid):
        """Save image with metadata"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        
        if pose_valid or self.config.SAVE_ALL_IMAGES:
            filename = f"frame_{frame_id:06d}_{timestamp}_valid{int(pose_valid)}.jpg"
            filepath = self.images_dir / filename
            cv2.imwrite(str(filepath), image)
        
        if not pose_valid and self.config.SAVE_FAILED_IMAGES:
            filename = f"failed_{frame_id:06d}_{timestamp}.jpg"
            filepath = self.failed_dir / filename
            cv2.imwrite(str(filepath), image)
    
    def handle_client(self, conn, addr):
        """Handle client connection"""
        self.log_message('INFO', f'Client connected: {addr}')
        
        try:
            while True:
                # Receive frame header
                header_data = self._recv_exact(conn, FrameHeader.SIZE)
                if not header_data:
                    break
                
                header = FrameHeader(header_data)
                
                if not header.is_valid():
                    self.log_message('ERROR', f'Invalid header magic: 0x{header.magic:08X}')
                    continue
                
                self.log_message('INFO', f'Receiving frame {header.frame_id}, '
                                f'size {header.data_size} bytes '
                                f'({header.width}x{header.height})')
                
                # Receive frame data
                frame_data = self._recv_exact(conn, header.data_size)
                if not frame_data:
                    break
                
                # Verify checksum
                calculated_checksum = sum(frame_data) & 0xFFFFFFFF
                if calculated_checksum != header.checksum:
                    self.log_message('WARNING', f'Checksum mismatch: '
                                    f'expected {header.checksum}, got {calculated_checksum}')
                
                # Convert to image
                image = self._decode_frame(frame_data, header.width, header.height)
                
                if image is None:
                    self.log_message('ERROR', 'Failed to decode frame')
                    continue
                
                # Process frame
                marker_found, marker_id, pose_valid, pose_data, annotated = \
                    self.verifier.detect_and_verify(image)
                
                self.total_frames += 1
                
                # Check for consecutive verification
                unlock_ready = 0
                if pose_valid:
                    self.valid_frames += 1
                    if self.verification_state.add_valid_frame(pose_data):
                        unlock_ready = 1
                        self.verified_sequences += 1
                        self.log_message('SUCCESS', f'✓ VERIFICATION COMPLETE '
                                        f'(sequence #{self.verified_sequences})')
                else:
                    self.verification_state.reset()
                
                # Create response
                response = PoseResponse()
                response.marker_found = 1 if marker_found else 0
                response.marker_id = marker_id
                response.pose_valid = 1 if pose_valid else 0
                response.unlock_ready = unlock_ready
                
                if pose_data:
                    response.pos_x = pose_data['pos_x_mm'] / 1000.0  # Convert to meters
                    response.pos_y = pose_data['pos_y_mm'] / 1000.0
                    response.pos_z = pose_data['pos_z_mm'] / 1000.0
                    response.rot_x = pose_data['rot_x_deg']
                    response.rot_y = pose_data['rot_y_deg']
                    response.rot_z = pose_data['rot_z_deg']
                
                # Send response
                conn.send(response.pack())
                
                # Log transaction
                request_info = {
                    'frame_id': header.frame_id,
                    'width': header.width,
                    'height': header.height,
                    'checksum': header.checksum
                }
                
                response_info = {
                    'marker_found': marker_found,
                    'marker_id': marker_id,
                    'pose_valid': pose_valid,
                    'unlock_ready': unlock_ready,
                    'pose_data': pose_data
                }
                
                self.log_transaction(header.frame_id, request_info, response_info)
                
                # Save image
                self.save_image(annotated, header.frame_id, marker_found, pose_valid)
                
                # Log summary
                self.log_message('INFO', 
                    f'Frame {header.frame_id}: '
                    f'Marker={marker_found}, Valid={pose_valid}, Unlock={unlock_ready} '
                    f'[Total: {self.total_frames}, Valid: {self.valid_frames}, '
                    f'Verified: {self.verified_sequences}]')
        
        except Exception as e:
            self.log_message('ERROR', f'Client error: {e}')
            import traceback
            traceback.print_exc()
        
        finally:
            conn.close()
            self.log_message('INFO', f'Client disconnected: {addr}')
    
    def _recv_exact(self, conn, size):
        """Receive exactly size bytes"""
        data = b''
        while len(data) < size:
            chunk = conn.recv(size - len(data))
            if not chunk:
                return None
            data += chunk
        return data
    
    def _decode_frame(self, data, width, height):
        """Decode YUV422 frame to BGR"""
        try:
            # YUV422 has 2 bytes per pixel
            expected_size = width * height * 2
            if len(data) != expected_size:
                self.log_message('WARNING', 
                    f'Frame size mismatch: expected {expected_size}, got {len(data)}')
                return None
            
            # Convert to numpy array
            yuv = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 2))
            
            # Convert YUV422 to BGR
            bgr = cv2.cvtColor(yuv, cv2.COLOR_YUV2BGR_YUYV)
            
            return bgr
        
        except Exception as e:
            self.log_message('ERROR', f'Frame decode error: {e}')
            return None
    
    def run(self):
        """Start server"""
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((self.config.HOST, self.config.PORT))
        server_socket.listen(1)
        
        print(f"\n{'='*70}")
        print(f"  ArUco Verification Server")
        print(f"{'='*70}")
        print(f"  Listening on {self.config.HOST}:{self.config.PORT}")
        print(f"  Target Marker ID: {self.config.TARGET_MARKER_ID}")
        print(f"  Logs: {self.log_dir}")
        print(f"{'='*70}\n")
        
        self.log_message('INFO', 'Server started')
        
        try:
            while True:
                conn, addr = server_socket.accept()
                client_thread = threading.Thread(
                    target=self.handle_client,
                    args=(conn, addr)
                )
                client_thread.start()
        
        except KeyboardInterrupt:
            print("\n[Server] Shutting down...")
            self.log_message('INFO', 'Server shutdown')
        
        finally:
            server_socket.close()


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description='ArUco Verification Server')
    parser.add_argument('--port', type=int, default=8888, help='Server port')
    parser.add_argument('--marker-id', type=int, default=0, help='Target marker ID')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Update config
    Config.PORT = args.port
    Config.TARGET_MARKER_ID = args.marker_id
    Config.VERBOSE = args.verbose
    
    # Create and run server
    server = ArucoServer(Config)
    server.run()


if __name__ == '__main__':
    main()
