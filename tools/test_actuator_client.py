#!/usr/bin/env python3
"""
Server script to wait for Pico actuator to connect, then send unlock command.
"""

import socket
import time

LISTEN_IP = "0.0.0.0"  # Listen on all interfaces
ACTUATOR_PORT = 9999
CMD_UNLOCK = 0x01

def main():
    print(f"Waiting for actuator Pico to connect on port {ACTUATOR_PORT}...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_sock:
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind((LISTEN_IP, ACTUATOR_PORT))
        server_sock.listen(1)
        conn, addr = server_sock.accept()
        with conn:
            print(f"✓ Pico connected from {addr}")
            time.sleep(0.5)
            print(f"Sending UNLOCK command (0x{CMD_UNLOCK:02X})...")
            conn.send(bytes([CMD_UNLOCK]))
            print("✓ Sent!")
            time.sleep(2)
            print("Done! Check your Pico's serial output.")

if __name__ == "__main__":
    main()
