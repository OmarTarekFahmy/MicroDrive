#!/usr/bin/env python3
"""
Simple TCP Server Test
Just listens on port 8888 and prints any connections
Use this to verify Pico W can reach your laptop
"""

import socket
import sys

HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 8888

print("="*60)
print("  Simple TCP Server - Connection Test")
print("="*60)
print(f"  Listening on {HOST}:{PORT}")
print(f"  Waiting for Pico W to connect...")
print("="*60)
print()

# Create socket
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

try:
    server_socket.bind((HOST, PORT))
    server_socket.listen(1)
    
    print("[OK] Server started successfully!")
    print("[OK] Port 8888 is open and listening")
    print()
    print("Now flash your Pico W and watch for connections...")
    print("Press Ctrl+C to stop")
    print()
    
    while True:
        # Wait for connection
        conn, addr = server_socket.accept()
        
        print("="*60)
        print(f"✓ CONNECTION RECEIVED!")
        print(f"  From: {addr[0]}:{addr[1]}")
        print(f"  This is your Pico W's IP address: {addr[0]}")
        print("="*60)
        print()
        
        try:
            # Receive some data
            data = conn.recv(1024)
            if data:
                print(f"[DATA] Received {len(data)} bytes")
                print(f"[DATA] First 32 bytes (hex): {data[:32].hex()}")
                print()
                
                # Send simple response
                response = b"OK from laptop!"
                conn.send(response)
                print(f"[SENT] Sent response: {response}")
            
        except Exception as e:
            print(f"[ERROR] {e}")
        
        finally:
            conn.close()
            print("[INFO] Connection closed")
            print()

except KeyboardInterrupt:
    print("\n[INFO] Server stopped by user")
    
except Exception as e:
    print(f"\n[ERROR] Failed to start server: {e}")
    print("\nCommon issues:")
    print("  - Port 8888 already in use (close other programs)")
    print("  - Firewall blocking port 8888")
    print("  - Permission denied (try running as administrator)")
    sys.exit(1)
    
finally:
    server_socket.close()
    print("[INFO] Server closed")
