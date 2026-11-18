#!/usr/bin/python3
import os, sys
sys.stderr.buffer.write(b'ERR\n' * 65536)
sys.stderr.flush()
sys.stderr.close()
os.close(2)
sys.stdout.buffer.write(b'OUT\n' * 65536)
sys.stdout.flush()
sys.exit(7)
