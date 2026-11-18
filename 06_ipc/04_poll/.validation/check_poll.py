import os
import re
import selectors
import signal
import subprocess
import time

def check_complete(output):
    chunks = re.findall(rb'parent: read=\d+, total=\d+, data="([A-Z]*)"', output)
    assert b"".join(chunks) == b"ABCDEFGHIJKLMNOP", output
    assert b"parent: read EOF, total=16" in output, output
    assert b"parent: child exit code=0" in output, output
    assert b"parent: reaped pid=" in output, output
    assert b"POLLERR" not in output and b"POLLNVAL" not in output, output

proc = subprocess.Popen(
    ["./poll_demo_host"], stdin=subprocess.PIPE,
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True,
)
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)
output = b""

def receive_until(predicate):
    global output
    deadline = time.monotonic() + 5
    while not predicate(output):
        assert time.monotonic() < deadline, output
        for key, _ in selector.select(timeout=0.1):
            chunk = os.read(key.fd, 4096)
            assert chunk, output
            output += chunk

try:
    receive_until(lambda data: b"poll timeout" in data and b"ENTER to send" in data)
    assert b"parent: read=" not in output
    proc.stdin.write(b"\n")
    proc.stdin.flush()
    receive_until(lambda data: b"total=16, data=" in data and
                  b"read would block; back to poll" in data and
                  b"ENTER again to close" in data)
    assert b"POLLIN" in output and b"read EOF" not in output
    assert proc.poll() is None
    remaining, errors = proc.communicate(b"\n", timeout=5)
    output += remaining
    assert proc.returncode == 0 and not errors, (output, errors)
    assert b"POLLHUP" in output, output
    check_complete(output)
    print("TWO STAGES\n" + output.decode(), end="")
finally:
    selector.close()
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate(timeout=5)

burst = subprocess.run(
    ["./poll_demo_host"], input=b"\n\n", capture_output=True, timeout=5
)
assert burst.returncode == 0 and not burst.stderr, (burst.stdout, burst.stderr)
check_complete(burst.stdout)
print("IMMEDIATE SEND/CLOSE\n" + burst.stdout.decode(), end="")
print("Passed: timeout, POLLIN, EAGAIN to poll, HUP/EOF, reaping, no lost data")
