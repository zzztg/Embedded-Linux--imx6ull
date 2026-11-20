import queue
import re
import signal
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

    def expect(self, text, timeout=5):
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
        code = self.proc.wait(timeout=20)
        self.thread.join(timeout=1)
        output = ''.join(self.output)
        assert code == expected, (code, output)
        return output


def server():
    p = Process('./buffered_server_native')
    p.expect('listening on 0.0.0.0:8894')
    return p


def client(mode=None):
    args = ['./build/host/buffered_client', '127.0.0.1']
    if mode:
        args.append(mode)
    return Process(*args)


def receive_all(peer):
    received = bytearray()
    while True:
        data = peer.recv(313)
        if not data:
            return bytes(received)
        received.extend(data)


try:
    s = server()
    a = client('hold')
    a.expect('press ENTER to shutdown SHUT_WR')
    assert 'received=1025; echo matches=yes' in client().finish()
    s.expect('client 2 closed; received=1025 queued=1025')
    assert a.proc.poll() is None and s.proc.poll() is None
    a.proc.stdin.write('\n')
    a.proc.stdin.flush()
    assert 'received=1025; echo matches=yes' in a.finish()
    output = s.finish()
    assert 'received_total=2050 queued_total=2050' in output, output
    assert output.index('client 2 closed') < output.index('client 1 closed'), output
    (ROOT.parent / 'hold_observation.txt').write_text(output, encoding='utf-8')
    print('PASS: two 1025-byte binary echoes; B finishes before held A; all exit 0')

    # Queue payload and FIN before the process accepts, making EOF-with-pending
    # deterministic instead of relying on cross-process scheduling speed.
    s = server()
    s.proc.send_signal(signal.SIGSTOP)
    payload = bytes(range(256)) * 4 + b'Z'
    with socket.create_connection(('127.0.0.1', 8894), timeout=10) as peer:
        peer.sendall(payload)
        peer.shutdown(socket.SHUT_WR)
        s.proc.send_signal(signal.SIGCONT)
        assert receive_all(peer) == payload
    assert 'received=0; echo matches=yes' in client('empty').finish()
    output = s.finish()
    assert re.search(r'client 1 input EOF; pending=[1-9][0-9]*', output), output
    assert 'output drained after EOF' in output, output
    assert 'received_total=1025 queued_total=1025' in output, output
    (ROOT.parent / 'eof_pending_observation.txt').write_text(output, encoding='utf-8')
    print('PASS: queued binary data plus FIN; EOF retains pending echo; empty second connection')

    s = server()
    payload = bytes((i * 7 + 19) % 256 for i in range(128 * 1024 + 17))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as peer:
        peer.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        peer.settimeout(15)
        peer.connect(('127.0.0.1', 8894))
        sender_errors = []

        def upload():
            try:
                peer.sendall(payload)
                peer.shutdown(socket.SHUT_WR)
            except Exception as error:
                sender_errors.append(error)

        uploader = threading.Thread(target=upload, daemon=True)
        uploader.start()
        s.expect('client 1 output buffer full')
        assert 'received=1025; echo matches=yes' in client().finish()
        s.expect('client 2 closed')
        assert s.proc.poll() is None
        assert not any('client 1 closed' in line for line in s.output)
        assert receive_all(peer) == payload
        uploader.join(timeout=2)
        assert not uploader.is_alive() and not sender_errors, sender_errors
    output = s.finish()
    assert f'received_total={len(payload) + 1025} queued_total={len(payload) + 1025}' in output
    print('PASS: slow reader with 128 KiB binary upload; B completes independently; no cross-client corruption')

    s = server()
    reset = socket.create_connection(('127.0.0.1', 8894), timeout=3)
    reset.sendall(b'R' * 1025)
    s.expect('client 1 output buffer full')
    reset.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    reset.close()
    s.expect('client 1 closed')
    assert 'received=1025; echo matches=yes' in client().finish()
    output = s.finish(expected=1)
    assert 'recv connection:' in output or 'send echo:' in output, output
    print('PASS: reset connection is cleaned up; next client succeeds; server reports error status 1')
finally:
    for item in PROCESSES:
        if item.proc.poll() is None:
            item.proc.send_signal(signal.SIGCONT)
            item.proc.terminate()
            try:
                item.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                item.proc.kill()
                item.proc.wait(timeout=2)
