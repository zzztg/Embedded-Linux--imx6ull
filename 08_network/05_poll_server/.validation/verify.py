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
        output = ''.join(self.output)
        assert code == expected, (code, output)
        return output


def server():
    p = Process('./poll_server_native')
    p.expect('listening on 0.0.0.0:8892')
    return p


def client(hold=False):
    args = ['./build/host/poll_client', '127.0.0.1']
    if hold:
        args.append('hold')
    return Process(*args)


try:
    s = server()
    a = client(hold=True)
    a.expect('press ENTER to close')
    b = client()
    assert 'echo matches=yes' in b.finish()
    s.expect('client 2 closed; active=1')
    assert a.proc.poll() is None and s.proc.poll() is None
    a.proc.stdin.write('\n')
    a.proc.stdin.flush()
    assert 'echo matches=yes' in a.finish()
    output = s.finish()
    assert 'accepted=2; echoed_total=32 bytes' in output, output
    assert output.index('client 2 EOF') < output.index('client 1 EOF'), output
    (ROOT.parent / 'hold_observation.txt').write_text(output, encoding='utf-8')
    print('PASS: B finishes while A remains held; server waits for A; total=32, all exit 0')

    s = server()
    assert 'echo matches=yes' in client().finish()
    s.expect('client 1 closed; active=0')
    assert s.proc.poll() is None
    assert 'echo matches=yes' in client().finish()
    output = s.finish()
    assert 'accepted=2; echoed_total=32 bytes' in output, output
    print('PASS: two sequential clients also work after a slot/fd is closed')

    s = server()
    payload = bytes(range(256)) * 4 + b'\x00'
    with socket.create_connection(('127.0.0.1', 8892), timeout=3) as peer:
        peer.sendall(payload)
        peer.shutdown(socket.SHUT_WR)
        received = bytearray()
        while True:
            data = peer.recv(113)
            if not data:
                break
            received.extend(data)
        assert received == payload
    with socket.create_connection(('127.0.0.1', 8892), timeout=3):
        pass
    output = s.finish()
    assert 'accepted=2; echoed_total=1025 bytes' in output, output
    print('PASS: binary data spans many receive rounds; EOF drains all data; empty second client')

    s = server()
    reset = socket.create_connection(('127.0.0.1', 8892), timeout=3)
    s.expect('accepted client 1')
    reset.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    reset.close()
    s.expect('client 1 closed; active=0')
    assert 'echo matches=yes' in client().finish()
    output = s.finish(expected=1)
    assert 'recv connection:' in output and 'echoed_total=16 bytes' in output, output
    print('PASS: reset error closes one connection; other client still served; error status 1')
finally:
    for item in PROCESSES:
        if item.proc.poll() is None:
            item.proc.terminate()
            try:
                item.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                item.proc.kill()
                item.proc.wait(timeout=2)
