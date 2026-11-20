import queue
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent / 'project'
PROCESSES = []
EXPECTED = 512 * 1024


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
    p = Process('./send_server_native')
    p.expect('listening on 0.0.0.0:8893')
    return p


def client(hold=False):
    args = ['./build/host/send_client', '127.0.0.1']
    if hold:
        args.append('hold')
    return Process(*args)


try:
    s = server()
    a = client(hold=True)
    a.expect('press ENTER to start receiving')
    s.expect('client 1 send would block')
    b = client()
    assert 'received=524288; pattern matches=yes' in b.finish()
    s.expect('client 2 closed; queued=524288/524288')
    assert a.proc.poll() is None and s.proc.poll() is None
    assert not any('client 1 closed' in line for line in s.output)
    a.proc.stdin.write('\n')
    a.proc.stdin.flush()
    assert 'received=524288; pattern matches=yes' in a.finish()
    output = s.finish()
    assert 'queued_total=1048576 bytes' in output, output
    assert output.index('client 2 closed') < output.index('client 1 closed'), output
    (ROOT.parent / 'backpressure_observation.txt').write_text(output, encoding='utf-8')
    print('PASS: A reaches EAGAIN; B completes while A holds; A resumes without data gaps; all exit 0')

    s = server()
    assert 'pattern matches=yes' in client().finish()
    assert s.proc.poll() is None
    assert 'pattern matches=yes' in client().finish()
    assert 'queued_total=1048576 bytes' in s.finish()
    print('PASS: two normal sequential downloads; each 512 KiB and exact byte pattern')

    s = server()
    reset = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    reset.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    reset.settimeout(3)
    reset.connect(('127.0.0.1', 8893))
    s.expect('client 1 send would block')
    reset.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    reset.close()
    s.expect('client 1 closed')
    assert 'pattern matches=yes' in client().finish()
    output = s.finish(expected=1)
    assert 'send response:' in output, output
    print('PASS: reset during pending send closes only that client; second succeeds; server reports 1')

    # Independently check the client's EOF/length and content validation.
    for response, label in [(b'ABC', 'truncated'), (b'Z' * EXPECTED, 'corrupt')]:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as fake:
            fake.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            fake.bind(('127.0.0.1', 8893))
            fake.listen(1)
            fake.settimeout(3)
            c = client()
            conn, _ = fake.accept()
            with conn:
                conn.sendall(response)
            assert 'pattern matches=no' in c.finish(expected=2)
        print('PASS: client rejects ' + label + ' response')
finally:
    for item in PROCESSES:
        if item.proc.poll() is None:
            item.proc.terminate()
            try:
                item.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                item.proc.kill()
                item.proc.wait(timeout=2)
