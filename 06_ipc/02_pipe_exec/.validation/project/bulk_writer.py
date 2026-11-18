#!/usr/bin/python3
import sys
sys.stdout.buffer.write(b'0123456789ABCDEF' * 65536)
sys.stdout.buffer.flush()
sys.exit(7)
