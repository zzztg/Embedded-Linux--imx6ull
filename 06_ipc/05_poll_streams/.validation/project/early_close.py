#!/usr/bin/python3
import os, sys
sys.stdout.write('early stdout\n')
sys.stdout.close()
os.close(1)
sys.stdin.buffer.readline()
sys.stderr.write('later stderr\n')
sys.stderr.flush()
sys.exit(7)
