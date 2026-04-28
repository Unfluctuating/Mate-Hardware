#!/usr/bin/env python3
"""
ROV Test Utility — UDP Client
Laptop IP  : 192.168.2.50
Teensy IP  : 192.168.2.177 : 8888
Listen port: 9999  (receives Teensy replies here)
"""

import socket
import threading
import sys

TEENSY_IP   = "192.168.2.177"
TEENSY_PORT = 8888   # where Teensy listens
MY_PORT     = 9999   # where THIS script listens for replies

def receive_thread(sock):
    """Continuously print data coming back from Teensy."""
    while True:
        try:
            data, _ = sock.recvfrom(4096)
            text = data.decode("utf-8", errors="replace")
            print(text, end="", flush=True)
        except OSError:
            break

def main():
    # Bind to 0.0.0.0:9999 so we receive Teensy replies
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", MY_PORT))
    sock.settimeout(None)

    print(f"[UDP Client] Bound to 0.0.0.0:{MY_PORT}")
    print(f"[UDP Client] Sending to {TEENSY_IP}:{TEENSY_PORT}")
    print("[UDP Client] Type commands and press Enter. Ctrl+C to quit.\n")

    # Start background thread for incoming data
    t = threading.Thread(target=receive_thread, args=(sock,), daemon=True)
    t.start()

    # Send an empty newline first to trigger the main menu
    sock.sendto(b"\n", (TEENSY_IP, TEENSY_PORT))

    try:
        while True:
            cmd = input()          # blocks until you type something
            msg = cmd + "\n"
            sock.sendto(msg.encode("utf-8"), (TEENSY_IP, TEENSY_PORT))
    except KeyboardInterrupt:
        print("\n[UDP Client] Exiting.")
        sock.close()
        sys.exit(0)

if __name__ == "__main__":
    main()