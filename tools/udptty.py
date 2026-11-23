"""The console's log: the loader's udptty (Neutrino, started from ORBIT) broadcasts the EE's printf on UDP 18194.

usage: python tools/udptty.py [out.log]     (on a PC of the console's network, while the game runs; Ctrl+C to stop)

Prints the runtime's BOOT, PERF and ERROR lines and appends them to out.log (default udptty.log).
"""
import socket
import sys

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("", 18194))
out = open(sys.argv[1] if len(sys.argv) > 1 else "udptty.log", "ab")
while True:
    d = s.recv(2048)
    sys.stdout.buffer.write(d)
    sys.stdout.flush()
    out.write(d)
    out.flush()
