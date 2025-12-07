#!/usr/bin/env python3
"""
ArUco Marker Verification Server
==================================
Receives camera frames from Pico W, detects ArUco markers,
verifies position/rotation/scale within tight tolerances for
2 consecutive frames. Logs all requests, responses, and images.

This version is cleaned up for the Pico OV7670 WiFi stream:
- Simple header + frame loop (no magic resync).
- Header matches the Pico C struct exactly.
- Y-only (grayscale) decode from YUV422.
"""

import socket
import struct
import cv2
import numpy as np
from datetime import datetime
import json
import threading
import time
from pathlib import Path
import argparse

# ============================================================================ #
# Configuration
# ============================================================================ #

class Config:
    """Server configuration with tolerance thresholds."""

    # Server settings
    HOST = "0.0.0.0"
    PORT = 8888

    # ArUco settings
    ARUCO_DICT = cv2.aruco.DICT_4X4_50
    TARGET_MARKER_ID = 42  # The marker ID we're looking for

    # Camera calibration (modify if you calibrate your camera)
    CAMERA_MATRIX = np.array(
        [
            [500,   0, 160],
            [  0, 500, 120],
            [  0,   0,   1],
        ],
        dtype=np.float32,
    )
    DIST_COEFFS = np.zeros((5, 1), dtype=np.float32)
    MARKER_SIZE = 0.05  # 5 cm

    # Multi-axis tolerance thresholds
    TOLERANCE_POSITION_X_MM = 25.0      # ±25mm horizontal position
    TOLERANCE_POSITION_Y_MM = 25.0      # ±25mm horizontal position
    TOLERANCE_POSITION_Z_MM = 30.0      # ±30mm vertical distance (height)
    TOLERANCE_ROTATION_DEG = 180.0      # ±180° tilt/orientation (X and Y axis) - very lenient
    TOLERANCE_YAW_DEG = 180.0           # ±180° yaw (Z axis rotation) - very lenient
    TOLERANCE_SCALE_PERCENT = 70.0      # ±70% scale variation

    # Target pose (reference) - Top-down view configuration
    TARGET_POS_X_MM = 0.0           # Centered horizontally
    TARGET_POS_Y_MM = 0.0           # Centered horizontally
    TARGET_POS_Z_MM = 200.0         # 20 cm from camera (height)
    TARGET_ROT_X_DEG = 0.0          # No tilt around X axis (pitch)
    TARGET_ROT_Y_DEG = 0.0          # No tilt around Y axis (roll)
    TARGET_ROT_Z_DEG = 0.0          # Cables pointing inward (yaw)

    # Consecutive verification requirements
    CONSECUTIVE_FRAMES_REQUIRED = 2  # Require 2 consecutive valid frames
    VERIFICATION_WINDOW_SECONDS = 2.0  # Within 2 second window

    # Logging
    LOG_DIR = "aruco_logs"
    SAVE_ALL_IMAGES = True
    SAVE_FAILED_IMAGES = True
    VERBOSE = True


# ============================================================================ #
# Data structures (wire protocol)
# ============================================================================ #

class FrameHeader:
    """
    Frame header structure matching Pico W.

    C struct on Pico side:

    typedef struct __attribute__((packed)) {
        uint32_t magic;     // 0xCAFEBABE
        uint32_t frame_id;
        uint16_t width;
        uint16_t height;
        uint16_t format;    // 0 = YUV422
        uint16_t reserved;  // 0
        uint32_t data_size; // image bytes
        uint32_t checksum;  // sum of bytes
    } frame_header_t;
    """

    MAGIC = 0xCAFEBABE
    FORMAT = "<IIHHHHII"
    SIZE = struct.calcsize(FORMAT)

    def __init__(self, data: bytes):
        (
            self.magic,
            self.frame_id,
            self.width,
            self.height,
            self.format,
            self.reserved,
            self.data_size,
            self.checksum,
        ) = struct.unpack(self.FORMAT, data)

    def is_valid(self) -> bool:
        return self.magic == self.MAGIC


class PoseResponse:
    """Response structure to send back to Pico W."""

    MAGIC = 0xDEADBEEF
    # magic, marker_found, marker_id, pose_valid, unlock_ready, pos_xyz, rot_xyz
    FORMAT = "<IBBBBffffff"
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

    def pack(self) -> bytes:
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
            self.rot_z,
        )


class VerificationState:
    """Tracks consecutive valid frames for verification."""

    def __init__(self):
        self.valid_frames = []  # list of (timestamp, pose_data)
        self.last_verification_time = None
        self.total_verifications = 0
        self.lock = threading.Lock()

    def add_valid_frame(self, pose_data):
        """Add a valid frame and check if verification is complete."""
        with self.lock:
            now = time.time()
            self.valid_frames.append((now, pose_data))

            cutoff = now - Config.VERIFICATION_WINDOW_SECONDS
            self.valid_frames = [(t, p) for (t, p) in self.valid_frames if t >= cutoff]

            if len(self.valid_frames) >= Config.CONSECUTIVE_FRAMES_REQUIRED:
                self.last_verification_time = now
                self.total_verifications += 1
                return True
            return False

    def reset(self):
        with self.lock:
            self.valid_frames = []

    def is_verified(self) -> bool:
        with self.lock:
            return len(self.valid_frames) >= Config.CONSECUTIVE_FRAMES_REQUIRED


# ============================================================================ #
# ArUco detection and pose verification
# ============================================================================ #

class ArucoVerifier:
    """ArUco marker detection and pose verification."""

    def __init__(self, config: Config):
        self.config = config
        self.aruco_dict = cv2.aruco.getPredefinedDictionary(config.ARUCO_DICT)
        self.aruco_params = cv2.aruco.DetectorParameters()
        self.detector = cv2.aruco.ArucoDetector(self.aruco_dict, self.aruco_params)

        print(f"[ArUco] Initialized detector for marker ID {config.TARGET_MARKER_ID}")
        print(f"[ArUco] Camera: Top-down view, cables pointing inward")
        print(
            f"[ArUco] Tolerance: Pos ±({config.TOLERANCE_POSITION_X_MM}, "
            f"{config.TOLERANCE_POSITION_Y_MM}, {config.TOLERANCE_POSITION_Z_MM}) mm"
        )
        print(f"[ArUco] Tolerance: Tilt ±{config.TOLERANCE_ROTATION_DEG}° Yaw ±{config.TOLERANCE_YAW_DEG}°")
        print(f"[ArUco] Tolerance: Scale ±{config.TOLERANCE_SCALE_PERCENT} %%")
        print(
            f"[ArUco] Verification: {config.CONSECUTIVE_FRAMES_REQUIRED} frames "
            f"within {config.VERIFICATION_WINDOW_SECONDS} s"
        )

    def detect_and_verify(self, image):
        """
        Detect ArUco marker and verify pose.

        Returns:
            (marker_found, marker_id, pose_valid, pose_data, annotated_image)
        """
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

        corners, ids, rejected = self.detector.detectMarkers(gray)
        annotated = image.copy()

        if ids is None or len(ids) == 0:
            print(f"[ARUCO] No markers detected in frame")
            return False, 0, False, None, annotated

        print(f"[ARUCO] Detected {len(ids)} marker(s): {ids.flatten().tolist()}")
        cv2.aruco.drawDetectedMarkers(annotated, corners, ids)

        # Find target marker
        target_idx = None
        for idx, marker_id in enumerate(ids.flatten()):
            if marker_id == self.config.TARGET_MARKER_ID:
                target_idx = idx
                print(f"[ARUCO] ✓ Found target marker ID {self.config.TARGET_MARKER_ID} at index {idx}")
                break

        if target_idx is None:
            # some marker found, but not the target ID
            print(f"[ARUCO] ✗ Target marker ID {self.config.TARGET_MARKER_ID} NOT FOUND. Detected: {ids.flatten().tolist()}")
            return True, int(ids[0][0]), False, None, annotated

        # Pose estimation
        rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(
            corners,
            self.config.MARKER_SIZE,
            self.config.CAMERA_MATRIX,
            self.config.DIST_COEFFS,
        )

        rvec = rvecs[target_idx][0]
        tvec = tvecs[target_idx][0]

        cv2.drawFrameAxes(
            annotated,
            self.config.CAMERA_MATRIX,
            self.config.DIST_COEFFS,
            rvec,
            tvec,
            self.config.MARKER_SIZE * 0.5,
        )

        pos_x_mm = tvec[0] * 1000.0
        pos_y_mm = tvec[1] * 1000.0
        pos_z_mm = tvec[2] * 1000.0

        # rotation matrix -> Euler
        rmat, _ = cv2.Rodrigues(rvec)
        rot_x_deg, rot_y_deg, rot_z_deg = self._rotation_matrix_to_euler(rmat)

        # approximate scale from distance between opposite corners
        corner_pts = corners[target_idx][0]
        corner_dist = np.linalg.norm(corner_pts[0] - corner_pts[2])
        expected_size = 100.0  # arbitrary reference at target distance
        scale_percent = (corner_dist / expected_size) * 100.0

        pose_data = {
            "pos_x_mm": pos_x_mm,
            "pos_y_mm": pos_y_mm,
            "pos_z_mm": pos_z_mm,
            "rot_x_deg": rot_x_deg,
            "rot_y_deg": rot_y_deg,
            "rot_z_deg": rot_z_deg,
            "scale_percent": scale_percent,
        }

        pose_valid = self._check_tolerances(pose_data)

        status_color = (0, 255, 0) if pose_valid else (0, 0, 255)
        status_text = "VALID" if pose_valid else "OUT OF TOLERANCE"

        cv2.putText(
            annotated,
            f"Marker {self.config.TARGET_MARKER_ID}: {status_text}",
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            status_color,
            2,
        )
        cv2.putText(
            annotated,
            f"Pos: ({pos_x_mm:.1f}, {pos_y_mm:.1f}, {pos_z_mm:.1f}) mm",
            (10, 60),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (255, 255, 255),
            1,
        )
        cv2.putText(
            annotated,
            f"Tilt: X={rot_x_deg:.1f}° Y={rot_y_deg:.1f}° Yaw={rot_z_deg:.1f}°",
            (10, 85),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (255, 255, 255),
            1,
        )
        cv2.putText(
            annotated,
            f"Scale: {scale_percent:.1f} %",
            (10, 110),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (255, 255, 255),
            1,
        )

        return True, self.config.TARGET_MARKER_ID, pose_valid, pose_data, annotated

    def _check_tolerances(self, pose_data) -> bool:
        cfg = self.config
        print(f"\n[TOLERANCE CHECK] Starting tolerance verification...")

        dx = abs(pose_data["pos_x_mm"] - cfg.TARGET_POS_X_MM)
        dy = abs(pose_data["pos_y_mm"] - cfg.TARGET_POS_Y_MM)
        dz = abs(pose_data["pos_z_mm"] - cfg.TARGET_POS_Z_MM)

        print(f"[POSITION] X: {pose_data['pos_x_mm']:.1f}mm (target: {cfg.TARGET_POS_X_MM:.1f}mm, diff: {dx:.1f}mm, limit: ±{cfg.TOLERANCE_POSITION_X_MM:.1f}mm) {'✓ PASS' if dx <= cfg.TOLERANCE_POSITION_X_MM else '✗ FAIL'}")
        print(f"[POSITION] Y: {pose_data['pos_y_mm']:.1f}mm (target: {cfg.TARGET_POS_Y_MM:.1f}mm, diff: {dy:.1f}mm, limit: ±{cfg.TOLERANCE_POSITION_Y_MM:.1f}mm) {'✓ PASS' if dy <= cfg.TOLERANCE_POSITION_Y_MM else '✗ FAIL'}")
        print(f"[POSITION] Z: {pose_data['pos_z_mm']:.1f}mm (target: {cfg.TARGET_POS_Z_MM:.1f}mm, diff: {dz:.1f}mm, limit: ±{cfg.TOLERANCE_POSITION_Z_MM:.1f}mm) {'✓ PASS' if dz <= cfg.TOLERANCE_POSITION_Z_MM else '✗ FAIL'}")

        if dx > cfg.TOLERANCE_POSITION_X_MM:
            print(f"[TOLERANCE CHECK] ✗ FAILED: X position out of tolerance")
            return False
        if dy > cfg.TOLERANCE_POSITION_Y_MM:
            print(f"[TOLERANCE CHECK] ✗ FAILED: Y position out of tolerance")
            return False
        if dz > cfg.TOLERANCE_POSITION_Z_MM:
            print(f"[TOLERANCE CHECK] ✗ FAILED: Z position out of tolerance")
            return False

        # Check tilt (X and Y axis - pitch and roll)
        drx = abs(pose_data["rot_x_deg"] - cfg.TARGET_ROT_X_DEG)
        dry = abs(pose_data["rot_y_deg"] - cfg.TARGET_ROT_Y_DEG)
        
        print(f"[TILT] X (pitch): {pose_data['rot_x_deg']:.1f}° (target: {cfg.TARGET_ROT_X_DEG:.1f}°, diff: {drx:.1f}°, limit: ±{cfg.TOLERANCE_ROTATION_DEG:.1f}°) {'✓ PASS' if drx <= cfg.TOLERANCE_ROTATION_DEG else '✗ FAIL'}")
        print(f"[TILT] Y (roll): {pose_data['rot_y_deg']:.1f}° (target: {cfg.TARGET_ROT_Y_DEG:.1f}°, diff: {dry:.1f}°, limit: ±{cfg.TOLERANCE_ROTATION_DEG:.1f}°) {'✓ PASS' if dry <= cfg.TOLERANCE_ROTATION_DEG else '✗ FAIL'}")
        
        if drx > cfg.TOLERANCE_ROTATION_DEG:
            print(f"[TOLERANCE CHECK] ✗ FAILED: X tilt (pitch) out of tolerance")
            return False
        if dry > cfg.TOLERANCE_ROTATION_DEG:
            print(f"[TOLERANCE CHECK] ✗ FAILED: Y tilt (roll) out of tolerance")
            return False
        
        # Check yaw (Z axis - rotation in plane) with separate tolerance
        drz = abs(pose_data["rot_z_deg"] - cfg.TARGET_ROT_Z_DEG)
        print(f"[YAW] Z (rotation): {pose_data['rot_z_deg']:.1f}° (target: {cfg.TARGET_ROT_Z_DEG:.1f}°, diff: {drz:.1f}°, limit: ±{cfg.TOLERANCE_YAW_DEG:.1f}°) {'✓ PASS' if drz <= cfg.TOLERANCE_YAW_DEG else '✗ FAIL'}")
        
        if drz > cfg.TOLERANCE_YAW_DEG:
            print(f"[TOLERANCE CHECK] ✗ FAILED: Z yaw (rotation) out of tolerance")
            return False

        dscale = abs(pose_data["scale_percent"] - 100.0)
        print(f"[SCALE] {pose_data['scale_percent']:.1f}% (target: 100.0%, diff: {dscale:.1f}%, limit: ±{cfg.TOLERANCE_SCALE_PERCENT:.1f}%) {'✓ PASS' if dscale <= cfg.TOLERANCE_SCALE_PERCENT else '✗ FAIL'}")
        
        if dscale > cfg.TOLERANCE_SCALE_PERCENT:
            print(f"[TOLERANCE CHECK] ✗ FAILED: Scale out of tolerance")
            return False

        print(f"[TOLERANCE CHECK] ✓✓✓ ALL CHECKS PASSED ✓✓✓\n")
        return True

    def _rotation_matrix_to_euler(self, R):
        """Convert rotation matrix to Euler angles (XYZ convention) in degrees."""
        sy = np.sqrt(R[0, 0] ** 2 + R[1, 0] ** 2)
        singular = sy < 1e-6

        if not singular:
            x = np.arctan2(R[2, 1], R[2, 2])
            y = np.arctan2(-R[2, 0], sy)
            z = np.arctan2(R[1, 0], R[0, 0])
        else:
            x = np.arctan2(-R[1, 2], R[1, 1])
            y = np.arctan2(-R[2, 0], sy)
            z = 0.0

        return np.degrees(x), np.degrees(y), np.degrees(z)


# ============================================================================ #
# Server
# ============================================================================ #

class ArucoServer:
    """TCP server for receiving frames and sending verification results."""

    def __init__(self, config: Config):
        self.config = config
        self.verifier = ArucoVerifier(config)
        self.verification_state = VerificationState()
        self.setup_logging()

        self.total_frames = 0
        self.valid_frames = 0
        self.verified_sequences = 0

    # -------- Logging setup -------- #

    def setup_logging(self):
        self.log_dir = Path(self.config.LOG_DIR)
        self.log_dir.mkdir(exist_ok=True)

        self.images_dir = self.log_dir / "images"
        self.failed_dir = self.log_dir / "failed"
        self.images_dir.mkdir(exist_ok=True)
        self.failed_dir.mkdir(exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_file = self.log_dir / f"session_{timestamp}.log"
        self.json_log_file = self.log_dir / f"session_{timestamp}.json"
        self.log_data = []

        print(f"[Server] Logging to: {self.log_file}")
        print(f"[Server] Images saved to: {self.images_dir}")

    def log_message(self, level: str, message: str):
        ts = datetime.now().isoformat()
        entry = f"[{ts}] [{level}] {message}"
        with open(self.log_file, "a", encoding="utf-8") as f:
            f.write(entry + "\n")
        if self.config.VERBOSE or level == "ERROR":
            print(entry)

    def log_transaction(self, frame_id, request_data, response_data):
        # Convert numpy types to native Python types for JSON serialization
        def convert_numpy(obj):
            if isinstance(obj, dict):
                return {k: convert_numpy(v) for k, v in obj.items()}
            elif isinstance(obj, (list, tuple)):
                return [convert_numpy(item) for item in obj]
            elif isinstance(obj, (np.integer, np.int64, np.int32)):
                return int(obj)
            elif isinstance(obj, (np.floating, np.float64, np.float32)):
                return float(obj)
            elif isinstance(obj, np.ndarray):
                return obj.tolist()
            return obj
        
        self.log_data.append(
            {
                "timestamp": datetime.now().isoformat(),
                "frame_id": frame_id,
                "request": convert_numpy(request_data),
                "response": convert_numpy(response_data),
            }
        )
        with open(self.json_log_file, "w", encoding="utf-8") as f:
            json.dump(self.log_data, f, indent=2)

    # -------- Networking helpers -------- #

    def _recv_exact(self, conn, size: int):
        """Receive exactly size bytes or None on disconnect."""
        data = b""
        while len(data) < size:
            chunk = conn.recv(size - len(data))
            if not chunk:
                print(f"[DEBUG] recv_exact: connection closed at {len(data)}/{size}")
                return None
            data += chunk
            if len(data) % 8192 == 0 or len(data) == size:
                print(f"[DEBUG] recv_exact: {len(data)}/{size}")
        return data

    # -------- Image decode -------- #

    def _decode_frame(self, data, width, height):
        """Decode YUV422 frame to BGR (robust to 1-byte misalignment)."""
        try:
            expected_size = width * height * 2
            if len(data) != expected_size:
                self.log_message(
                    'WARNING',
                    f'Frame size mismatch: expected {expected_size}, got {len(data)}'
                )
                return None

            buf = np.frombuffer(data, dtype=np.uint8)

            # Candidate 1: assume Y at even bytes (0,2,4,...) -> typical Y0 U0 Y1 V0 pattern
            try:
                y_even = buf[0::2].reshape((height, width))
            except ValueError:
                self.log_message('ERROR', 'Reshape failed for y_even')
                return None

            # Candidate 2: assume Y at odd bytes (1,3,5,...)
            try:
                y_odd = buf[1::2].reshape((height, width))
            except ValueError:
                self.log_message('ERROR', 'Reshape failed for y_odd')
                return None

            # Heuristic: "wrong" candidate (U/V) will have very strong vertical stripes
            # Measure horizontal gradient energy and pick the smoother one
            def grad_energy(img):
                # absolute diff between neighboring pixels horizontally
                gx = np.abs(np.diff(img.astype(np.int16), axis=1)).mean()
                return gx

            e_even = grad_energy(y_even)
            e_odd = grad_energy(y_odd)

            # The real Y image tends to be smoother horizontally than a pure U/V image.
            # If this flips in your case, just swap the condition.
            if e_even < e_odd:
                y = y_even
                chosen = "even"
            else:
                y = y_odd
                chosen = "odd"

            self.log_message('DEBUG', f'_decode_frame: chose {chosen} Y offset '
                                    f'(e_even={e_even:.2f}, e_odd={e_odd:.2f})')

            # Make 3-channel BGR from grayscale Y
            bgr = cv2.merge([y, y, y])
            return bgr

        except Exception as e:
            self.log_message('ERROR', f'Frame decode error: {e}')
            return None

        """Decode frame assuming UYVY-like YUV422, keep only Y (grayscale)."""
        try:
            expected_size = width * height * 2
            if len(data) != expected_size:
                self.log_message(
                    'WARNING',
                    f'Frame size mismatch: expected {expected_size}, got {len(data)}'
                )
                return None

            # Y is at odd positions: 1, 3, 5, ... (UYVY style)
            y = np.frombuffer(data, dtype=np.uint8)[1::2]

            if y.size != width * height:
                self.log_message(
                    'ERROR',
                    f'Grayscale size mismatch: expected {width*height}, got {y.size}'
                )
                return None

            y = y.reshape((height, width))

            # ArUco code expects BGR, so convert gray -> BGR
            bgr = cv2.cvtColor(y, cv2.COLOR_GRAY2BGR)

            return bgr

        except Exception as e:
            self.log_message('ERROR', f'Frame decode error: {e}')
            return None

    # -------- Image save -------- #

    def save_image(self, image, frame_id, marker_found, pose_valid):
        ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")

        if pose_valid or self.config.SAVE_ALL_IMAGES:
            fname = f"frame_{frame_id:06d}_{ts}_valid{int(pose_valid)}.png"
            path = self.images_dir / fname
            ok = cv2.imwrite(str(path), image)
            print(f"[SAVE] frame {frame_id}: wrote {path}, ok={ok}")

        if not pose_valid and self.config.SAVE_FAILED_IMAGES:
            fname = f"failed_{frame_id:06d}_{ts}.png"
            path = self.failed_dir / fname
            ok2 = cv2.imwrite(str(path), image)
            print(f"[SAVE] frame {frame_id}: wrote failed {path}, ok={ok2}")

    # -------- Per-frame processing -------- #

    def _process_frame(self, conn, header: FrameHeader) -> bool:
        print(
            f"[FRAME] Processing frame {header.frame_id}, "
            f"{header.width}x{header.height}, data_size={header.data_size}"
        )

        # Receive frame payload
        frame_data = self._recv_exact(conn, header.data_size)
        if not frame_data:
            print(f"[FRAME] frame_data recv failed for frame {header.frame_id}")
            return False

        # Verify checksum
        calc_checksum = sum(frame_data) & 0xFFFFFFFF
        if calc_checksum != header.checksum:
            self.log_message(
                "WARNING",
                f"Checksum mismatch: expected {header.checksum}, got {calc_checksum}",
            )

        # Decode to BGR
        image = self._decode_frame(frame_data, header.width, header.height)
        if image is None:
            print(f"[FRAME] decode FAILED for frame {header.frame_id}")
            return True  # keep listening for next frames

        print(f"[FRAME] decode OK for frame {header.frame_id}, shape={image.shape}")

        # Run ArUco
        marker_found, marker_id, pose_valid, pose_data, annotated = \
            self.verifier.detect_and_verify(image)

        self.total_frames += 1
        print(f"marker_found={marker_found}, marker_id={marker_id}, pose_valid={pose_valid}, pose_data={pose_data}, annotated_shape={annotated.shape}")
        unlock_ready = 0
        # marker_found=True, marker_id=42, pose_valid=False, pose_data=None, annotated_shape=(240, 320, 3)    
        if pose_valid:
            self.valid_frames += 1
            print(f"[VERIFICATION] Valid frame added. Current count: {len(self.verification_state.valid_frames) + 1}/{self.config.CONSECUTIVE_FRAMES_REQUIRED}")
            if self.verification_state.add_valid_frame(pose_data):
                unlock_ready = 1
                self.verified_sequences += 1
                print(f"\n{'='*70}")
                print(f"🎉 VERIFICATION COMPLETE! (sequence #{self.verified_sequences})")
                print(f"✓ {self.config.CONSECUTIVE_FRAMES_REQUIRED} consecutive valid frames detected within {self.config.VERIFICATION_WINDOW_SECONDS}s")
                print(f"✓ Marker ID {self.config.TARGET_MARKER_ID} verified")
                print(f"{'='*70}\n")
                self.log_message(
                    "SUCCESS",
                    f"✓ VERIFICATION COMPLETE (sequence #{self.verified_sequences})",
                )
        else:
            print(f"[VERIFICATION] Invalid frame - resetting consecutive frame counter")
            self.verification_state.reset()

        # Build and send response
        resp = PoseResponse()
        resp.marker_found = 1 if marker_found else 0
        resp.marker_id = int(marker_id) if marker_found else 0
        resp.pose_valid = 1 if pose_valid else 0
        resp.unlock_ready = unlock_ready

        if pose_data:
            resp.pos_x = pose_data["pos_x_mm"] / 1000.0
            resp.pos_y = pose_data["pos_y_mm"] / 1000.0
            resp.pos_z = pose_data["pos_z_mm"] / 1000.0
            resp.rot_x = pose_data["rot_x_deg"]
            resp.rot_y = pose_data["rot_y_deg"]
            resp.rot_z = pose_data["rot_z_deg"]

        try:
            conn.sendall(resp.pack())
        except Exception as e:
            self.log_message("ERROR", f"Failed to send response: {e}")
            return False

        # Log transaction
        req_info = {
            "frame_id": header.frame_id,
            "width": header.width,
            "height": header.height,
            "checksum": header.checksum,
        }
        resp_info = {
            "marker_found": marker_found,
            "marker_id": marker_id,
            "pose_valid": pose_valid,
            "unlock_ready": unlock_ready,
            "pose_data": pose_data,
        }
        self.log_transaction(header.frame_id, req_info, resp_info)

        # Save image
        self.save_image(annotated, header.frame_id, marker_found, pose_valid)

        self.log_message(
            "INFO",
            f"Frame {header.frame_id}: "
            f"Marker={marker_found}, Valid={pose_valid}, Unlock={unlock_ready} "
            f"[Total: {self.total_frames}, Valid: {self.valid_frames}, "
            f"Verified: {self.verified_sequences}]",
        )

        return True

    # -------- Client handling -------- #

    def handle_client(self, conn, addr):
        """Handle one TCP client: header + frame loop."""
        self.log_message("INFO", f"Client connected: {addr}")
        try:
            while True:
                # Read header
                header_data = self._recv_exact(conn, FrameHeader.SIZE)
                if not header_data:
                    print("[DEBUG] Connection closed while reading header")
                    break

                header = FrameHeader(header_data)
                if not header.is_valid():
                    self.log_message(
                        "ERROR",
                        f"Invalid header magic: 0x{header.magic:08X}",
                    )
                    break

                print(
                    f"[HEADER] id={header.frame_id}, {header.width}x{header.height}, "
                    f"data_size={header.data_size}, checksum={header.checksum}"
                )

                # Process frame (reads payload, decodes, responds)
                if not self._process_frame(conn, header):
                    print("[DEBUG] _process_frame returned False, stopping client loop")
                    break

        except Exception as e:
            self.log_message("ERROR", f"Client error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            conn.close()
            self.log_message("INFO", f"Client disconnected: {addr}")

    # -------- Main server loop -------- #

    def run(self):
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((self.config.HOST, self.config.PORT))
        server_socket.listen(1)

        print("\n" + "=" * 70)
        print("  ArUco Verification Server")
        print("=" * 70)
        print(f"  Listening on {self.config.HOST}:{self.config.PORT}")
        print(f"  Target Marker ID: {self.config.TARGET_MARKER_ID}")
        print(f"  Logs: {self.log_dir}")
        print("=" * 70 + "\n")

        self.log_message("INFO", "Server started")

        try:
            while True:
                conn, addr = server_socket.accept()
                t = threading.Thread(target=self.handle_client, args=(conn, addr))
                t.daemon = True
                t.start()
        except KeyboardInterrupt:
            print("\n[Server] Shutting down...")
            self.log_message("INFO", "Server shutdown")
        finally:
            server_socket.close()


# ============================================================================ #
# Main
# ============================================================================ #

def main():
    parser = argparse.ArgumentParser(description="ArUco Verification Server")
    parser.add_argument("--port", type=int, default=8888, help="Server port")
    parser.add_argument("--marker-id", type=int, default=42, help="Target marker ID (default: 42)")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")

    args = parser.parse_args()

    Config.PORT = args.port
    Config.TARGET_MARKER_ID = args.marker_id
    Config.VERBOSE = args.verbose

    server = ArucoServer(Config)
    server.run()


if __name__ == "__main__":
    main()
