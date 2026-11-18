import os
import re
import selectors
import signal
import subprocess
import time

def check_contents(text):
    chunks = re.findall(r'child: read=\d+, total=\d+, data="([A-Z]*)"', text)
    assert "".join(chunks) == "ABCDEFGHIJKLMNOP", text
    assert "child: read returned 0 (EOF), total=16" in text, text
    assert "parent: child exit code=0" in text, text
    assert "parent: reaped pid=" in text, text

normal = subprocess.run(
    ["./pipe_demo_host", "normal"], capture_output=True, text=True, timeout=5
)
assert normal.returncode == 0 and not normal.stderr, (normal.stdout, normal.stderr)
check_contents(normal.stdout)
print("NORMAL\n" + normal.stdout, end="")

proc = subprocess.Popen(
    ["./pipe_demo_host", "hold"], stdin=subprocess.PIPE,
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True,
)
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)
prefix = b""
try:
    deadline = time.monotonic() + 5
    while b"total=16, data=" not in prefix or b"press ENTER" not in prefix:
        assert time.monotonic() < deadline, prefix
        for key, _ in selector.select(timeout=0.1):
            chunk = os.read(key.fd, 4096)
            assert chunk, prefix
            prefix += chunk
    assert proc.poll() is None, prefix
    for key, _ in selector.select(timeout=0.15):
        prefix += os.read(key.fd, 4096)
    assert b"(EOF)" not in prefix, prefix
    print("HOLD before ENTER: all 16 bytes received, parent alive, no EOF")
    remaining, errors = proc.communicate(b"\n", timeout=5)
    complete = (prefix + remaining).decode()
    assert proc.returncode == 0 and not errors, (complete, errors)
    check_contents(complete)
    print("HOLD complete\n" + complete, end="")
finally:
    selector.close()
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate(timeout=5)
