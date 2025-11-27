"""
Camera Calibration Utility
==========================

Calibrates camera intrinsics using a chessboard pattern for more accurate
ArUco pose estimation. Use this if you notice pose estimation errors.

Usage:
    1. Print a chessboard pattern (9x6 inner corners)
    2. Run: python calibrate_camera.py
    3. Move chessboard around, capturing 15-20 images
    4. Press 'c' to capture, 'q' to finish and calibrate
    5. Use the generated calibration.npz with aruco_server.py

Requirements:
    pip install opencv-python numpy
"""

import cv2
import numpy as np
import sys
import os
from datetime import datetime


# Chessboard dimensions (inner corners)
CHESSBOARD_SIZE = (9, 6)  # Width x Height
SQUARE_SIZE = 0.025  # Size of each square in meters (25mm)

# Termination criteria for corner refinement
CRITERIA = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)


def calibrate_from_webcam(camera_id: int = 0, output_file: str = "calibration.npz"):
    """
    Interactive calibration from webcam
    """
    print("="*60)
    print("  Camera Calibration Utility")
    print("="*60)
    print(f"\n  Chessboard: {CHESSBOARD_SIZE[0]}x{CHESSBOARD_SIZE[1]} inner corners")
    print(f"  Square size: {SQUARE_SIZE*1000:.1f}mm")
    print("\n  Controls:")
    print("    'c' - Capture current frame")
    print("    'q' - Quit and calibrate")
    print("    's' - Skip (discard current detection)")
    print("\n  Aim for 15-20 captures from different angles")
    print("="*60)
    
    cap = cv2.VideoCapture(camera_id)
    if not cap.isOpened():
        print(f"ERROR: Cannot open camera {camera_id}")
        return False
    
    # Set resolution
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    
    # Prepare object points (0,0,0), (1,0,0), (2,0,0) ...
    objp = np.zeros((CHESSBOARD_SIZE[0] * CHESSBOARD_SIZE[1], 3), np.float32)
    objp[:, :2] = np.mgrid[0:CHESSBOARD_SIZE[0], 0:CHESSBOARD_SIZE[1]].T.reshape(-1, 2)
    objp *= SQUARE_SIZE
    
    # Storage for calibration points
    obj_points = []  # 3D points in real world
    img_points = []  # 2D points in image plane
    
    capture_count = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Find chessboard corners
        found, corners = cv2.findChessboardCorners(gray, CHESSBOARD_SIZE, None)
        
        display = frame.copy()
        
        if found:
            # Refine corner positions
            corners2 = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), CRITERIA)
            
            # Draw corners
            cv2.drawChessboardCorners(display, CHESSBOARD_SIZE, corners2, found)
            
            # Status text
            cv2.putText(display, "Chessboard FOUND - Press 'c' to capture",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        else:
            cv2.putText(display, "Searching for chessboard...",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        
        cv2.putText(display, f"Captures: {capture_count}/15 (press 'q' when done)",
                    (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)
        
        cv2.imshow("Camera Calibration", display)
        
        key = cv2.waitKey(1) & 0xFF
        
        if key == ord('c') and found:
            obj_points.append(objp)
            img_points.append(corners2)
            capture_count += 1
            print(f"[Capture {capture_count}] Added calibration frame")
            
        elif key == ord('q'):
            break
        elif key == ord('s'):
            print("[Skip] Discarded current frame")
    
    cap.release()
    cv2.destroyAllWindows()
    
    if capture_count < 10:
        print(f"\nWARNING: Only {capture_count} captures. 15+ recommended for accuracy.")
        if capture_count < 5:
            print("ERROR: Need at least 5 captures to calibrate")
            return False
    
    # Perform calibration
    print(f"\nCalibrating with {capture_count} images...")
    
    ret, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(
        obj_points, img_points, gray.shape[::-1], None, None
    )
    
    if ret:
        # Calculate reprojection error
        total_error = 0
        for i in range(len(obj_points)):
            img_points2, _ = cv2.projectPoints(obj_points[i], rvecs[i], tvecs[i],
                                                camera_matrix, dist_coeffs)
            error = cv2.norm(img_points[i], img_points2, cv2.NORM_L2) / len(img_points2)
            total_error += error
        
        mean_error = total_error / len(obj_points)
        
        print("\n" + "="*60)
        print("  Calibration Complete!")
        print("="*60)
        print(f"\n  Reprojection error: {mean_error:.4f} pixels")
        print("  (Good: < 0.5, Acceptable: < 1.0)")
        
        print(f"\n  Camera Matrix:")
        print(f"    fx = {camera_matrix[0,0]:.2f}")
        print(f"    fy = {camera_matrix[1,1]:.2f}")
        print(f"    cx = {camera_matrix[0,2]:.2f}")
        print(f"    cy = {camera_matrix[1,2]:.2f}")
        
        print(f"\n  Distortion Coefficients:")
        print(f"    k1={dist_coeffs[0,0]:.6f}, k2={dist_coeffs[1,0]:.6f}")
        print(f"    p1={dist_coeffs[2,0]:.6f}, p2={dist_coeffs[3,0]:.6f}")
        print(f"    k3={dist_coeffs[4,0]:.6f}")
        
        # Save calibration
        np.savez(output_file,
                 camera_matrix=camera_matrix,
                 dist_coeffs=dist_coeffs,
                 reprojection_error=mean_error)
        
        print(f"\n  Saved to: {output_file}")
        print("\n  To use with ArUco server:")
        print(f"    python aruco_server.py --calibration {output_file}")
        print("="*60)
        
        return True
    else:
        print("ERROR: Calibration failed!")
        return False


def calibrate_from_pico_stream(host: str = "0.0.0.0", port: int = 8888):
    """
    Calibration using frames from Pico W stream
    
    This requires the Pico to stream raw frames, which we capture
    for calibration purposes.
    """
    import socket
    import struct
    
    print("="*60)
    print("  Camera Calibration from Pico Stream")
    print("="*60)
    print(f"\n  Listening on port {port} for Pico connection...")
    
    # Start server
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(1)
    
    print("  Waiting for Pico W to connect...")
    conn, addr = server.accept()
    print(f"  Connected from {addr}")
    
    # Frame header format
    HEADER_FORMAT = '<IIHHHHI'
    HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
    FRAME_MAGIC = 0xCAFEBABE
    
    # Calibration storage
    objp = np.zeros((CHESSBOARD_SIZE[0] * CHESSBOARD_SIZE[1], 3), np.float32)
    objp[:, :2] = np.mgrid[0:CHESSBOARD_SIZE[0], 0:CHESSBOARD_SIZE[1]].T.reshape(-1, 2)
    objp *= SQUARE_SIZE
    
    obj_points = []
    img_points = []
    capture_count = 0
    
    try:
        while True:
            # Receive header
            header_data = b''
            while len(header_data) < HEADER_SIZE:
                chunk = conn.recv(HEADER_SIZE - len(header_data))
                if not chunk:
                    break
                header_data += chunk
            
            if len(header_data) < HEADER_SIZE:
                break
            
            magic, frame_id, width, height, fmt, size, checksum = \
                struct.unpack(HEADER_FORMAT, header_data)
            
            if magic != FRAME_MAGIC:
                continue
            
            # Receive frame data
            expected_size = width * height * 2
            frame_data = b''
            while len(frame_data) < expected_size:
                chunk = conn.recv(expected_size - len(frame_data))
                if not chunk:
                    break
                frame_data += chunk
            
            # Convert YUV422 to BGR
            yuv = np.frombuffer(frame_data, dtype=np.uint8).reshape((height, width, 2))
            frame = cv2.cvtColor(yuv, cv2.COLOR_YUV2BGR_YUYV)
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            
            # Find chessboard
            found, corners = cv2.findChessboardCorners(gray, CHESSBOARD_SIZE, None)
            
            display = frame.copy()
            
            if found:
                corners2 = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), CRITERIA)
                cv2.drawChessboardCorners(display, CHESSBOARD_SIZE, corners2, found)
                cv2.putText(display, "FOUND - Press 'c' to capture",
                            (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
            else:
                cv2.putText(display, "Searching...",
                            (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
            
            cv2.putText(display, f"Captures: {capture_count}",
                        (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
            
            cv2.imshow("Pico Calibration", display)
            
            key = cv2.waitKey(1) & 0xFF
            
            if key == ord('c') and found:
                obj_points.append(objp)
                img_points.append(corners2)
                capture_count += 1
                print(f"[Capture {capture_count}]")
                
            elif key == ord('q'):
                break
            
            # Send dummy response to keep connection alive
            response = struct.pack('<IBBBBffffff', 0xDEADBEEF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
            conn.send(response)
            
    finally:
        conn.close()
        server.close()
        cv2.destroyAllWindows()
    
    if capture_count >= 5:
        print(f"\nCalibrating with {capture_count} images...")
        ret, camera_matrix, dist_coeffs, _, _ = cv2.calibrateCamera(
            obj_points, img_points, gray.shape[::-1], None, None
        )
        
        if ret:
            np.savez("calibration_pico.npz",
                     camera_matrix=camera_matrix,
                     dist_coeffs=dist_coeffs)
            print(f"Saved: calibration_pico.npz")
            return True
    
    return False


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Camera Calibration Utility')
    parser.add_argument('--source', choices=['webcam', 'pico'], default='webcam',
                        help='Calibration source')
    parser.add_argument('--camera', type=int, default=0,
                        help='Webcam ID (default: 0)')
    parser.add_argument('--output', '-o', type=str, default='calibration.npz',
                        help='Output file')
    parser.add_argument('--port', type=int, default=8888,
                        help='Port for Pico stream')
    
    args = parser.parse_args()
    
    if args.source == 'webcam':
        calibrate_from_webcam(args.camera, args.output)
    else:
        calibrate_from_pico_stream(port=args.port)


if __name__ == '__main__':
    main()
