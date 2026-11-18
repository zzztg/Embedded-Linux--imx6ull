import queue
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent / 'project'
PROCESSES = []


class Process:
    def __init__(self, *command):
        self.output = []
        self.lines = queue.Queue()
        self.proc = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, bufsize=1)
        self.thread = threading.Thread(target=self.collect, daemon=True)
        self.thread.start()
        PROCESSES.append(self)

    def collect(self):
        for line in self.proc.stdout:
            self.output.append(line)
            self.lines.put(line)

    def expect(self, text, timeout=4):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            assert remaining > 0, (text, ''.join(self.output))
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty:
                raise AssertionError((text, ''.join(self.output)))
            if text in line:
                return

    def finish(self, expected=0):
        code = self.proc.wait(timeout=5)
        self.thread.join(timeout=1)
        assert code == expected, (code, ''.join(self.output))
        return ''.join(self.output)


def server():
    p = Process('./loop_server_native')
    p.expect('waiting for client 1/2')
    return p


def client(hold=False):
    args = ['./build/host/loop_client', '127.0.0.1']
    if hold:
        args.append('hold')
    return Process(*args)


try:
    s = server()
    assert 'echo matches=yes' in client().finish()
    s.expect('waiting for client 2/2')
    assert s.proc.poll() is None
    assert 'echo matches=yes' in client().finish()
    assert 'echoed_total=32' in s.finish()
    print('PASS: same server accepts two sequential clients; first EOF preserves listener')

    s = server()
    a = client(hold=True)
    a.expect('press ENTER to close')
    b = client()
    b.expect('sent 16 bytes; waiting for echo')
    # First client still holds its connection. Second has connected and sent,
    # but cannot receive an application echo while the server reads client 1.
    try:
        b.proc.wait(timeout=0.25)
        raise AssertionError('second client finished before first was released')
    except subprocess.TimeoutExpired:
        pass
    assert not any('received ' in line for line in b.output), b.output
    assert not any('accepted client 2' in line for line in s.output), s.output
    a.proc.stdin.write('\n')
    a.proc.stdin.flush()
    assert 'echo matches=yes' in a.finish()
    assert 'echo matches=yes' in b.finish()
    assert 'echoed_total=32' in s.finish()
    print('PASS: second client connects/sends but waits for first connection to close')

    s = server()
    with socket.create_connection(('127.0.0.1', 8891), timeout=3):
        pass
    s.expect('waiting for client 2/2')
    assert 'echo matches=yes' in client().finish()
    assert 'echoed_total=16' in s.finish()
    print('PASS: empty connection EOF does not stop next accept')

    s = server()
    reset = socket.create_connection(('127.0.0.1', 8891), timeout=3)
    s.expect('accepted client 1')
    reset.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    reset.close()
    s.expect('waiting for client 2/2')
    assert 'echo matches=yes' in client().finish()
    output = s.finish(expected=1)
    assert 'read connection:' in output and 'echoed_total=16' in output, output
    print('PASS: reset connection is closed; next client served; server reports prior error')
finally:
    for item in PROCESSES:
        if item.proc.poll() is None:
            item.proc.terminate()
            try:
                item.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                item.proc.kill()
                item.proc.wait(timeout=2)
