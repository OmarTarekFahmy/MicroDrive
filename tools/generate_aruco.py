"""
ArUco Marker Generator
======================

Generate printable ArUco markers for the MicroDrive unlock system.

Usage:
    python generate_aruco.py [--id 42] [--size 200] [--output marker.png]
"""

import argparse
import sys

try:
    import cv2
    from cv2 import aruco
    import numpy as np
except ImportError:
    print("ERROR: OpenCV not found. Install with:")
    print("  pip install opencv-python opencv-contrib-python")
    sys.exit(1)


def generate_marker(marker_id: int, size: int, output_path: str, dictionary: int = aruco.DICT_4X4_50):
    """
    Generate an ArUco marker image
    
    Args:
        marker_id: ID of the marker (0-49 for DICT_4X4_50)
        size: Size of the marker in pixels
        output_path: Output file path
        dictionary: ArUco dictionary type
    """
    # Get dictionary
    aruco_dict = aruco.getPredefinedDictionary(dictionary)
    
    # Generate marker
    marker_image = aruco.generateImageMarker(aruco_dict, marker_id, size)
    
    # Add white border for easier detection
    border_size = size // 10
    bordered = cv2.copyMakeBorder(
        marker_image,
        border_size, border_size, border_size, border_size,
        cv2.BORDER_CONSTANT,
        value=255
    )
    
    # Save image
    cv2.imwrite(output_path, bordered)
    print(f"Generated ArUco marker ID {marker_id}")
    print(f"  Size: {bordered.shape[1]}x{bordered.shape[0]} pixels")
    print(f"  Saved to: {output_path}")
    
    return bordered


def generate_marker_sheet(marker_ids: list, size: int, output_path: str):
    """Generate a sheet of multiple markers"""
    
    aruco_dict = aruco.getPredefinedDictionary(aruco.DICT_4X4_50)
    
    # Calculate grid
    n_markers = len(marker_ids)
    cols = min(4, n_markers)
    rows = (n_markers + cols - 1) // cols
    
    # Create sheet
    border = size // 10
    cell_size = size + 2 * border
    sheet_width = cols * cell_size + border
    sheet_height = rows * cell_size + border
    
    sheet = np.ones((sheet_height, sheet_width), dtype=np.uint8) * 255
    
    for i, marker_id in enumerate(marker_ids):
        row = i // cols
        col = i % cols
        
        # Generate marker
        marker = aruco.generateImageMarker(aruco_dict, marker_id, size)
        
        # Place on sheet
        y = row * cell_size + border
        x = col * cell_size + border
        sheet[y:y+size, x:x+size] = marker
        
        # Add ID label
        cv2.putText(sheet, f"ID: {marker_id}",
                    (x, y + size + border - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, 0, 1)
    
    cv2.imwrite(output_path, sheet)
    print(f"Generated marker sheet with IDs: {marker_ids}")
    print(f"  Saved to: {output_path}")
    
    return sheet


def main():
    parser = argparse.ArgumentParser(description='Generate ArUco markers')
    parser.add_argument('--id', type=int, default=42,
                        help='Marker ID (default: 42)')
    parser.add_argument('--size', type=int, default=200,
                        help='Marker size in pixels (default: 200)')
    parser.add_argument('--output', '-o', type=str, default='aruco_marker.png',
                        help='Output file path')
    parser.add_argument('--sheet', type=int, nargs='+',
                        help='Generate a sheet with multiple marker IDs')
    parser.add_argument('--show', action='store_true',
                        help='Display the generated marker')
    
    args = parser.parse_args()
    
    if args.sheet:
        marker = generate_marker_sheet(args.sheet, args.size, args.output)
    else:
        marker = generate_marker(args.id, args.size, args.output)
    
    if args.show:
        cv2.imshow("ArUco Marker", marker)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    
    print("\nPrinting Instructions:")
    print("  1. Print at actual size (no scaling)")
    print("  2. Use matte paper to avoid reflections")
    print("  3. Ensure marker is flat when mounted")
    print(f"  4. Measure printed marker and update --marker-size in server")


if __name__ == '__main__':
    main()
