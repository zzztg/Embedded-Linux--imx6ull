from pathlib import Path
import os
import selectors
import subprocess
import time

program = Path(__file__).resolve().parent / "project" / "cond_demo_host"
proc = subprocess.Popen([str(program)], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
output = bytearray()
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)
waiting = b"worker: ready=0; waiting (wait releases mutex)\n"


def observe_until(predicate):
    deadline = time.monotonic() + 5
    while not predicate(bytes(output)):
        remaining = deadline - time.monotonic()
        assert remaining > 0, ("stage timed out", bytes(output))
        events = selector.select(remaining)
        assert events, ("no output before deadline", bytes(output))
        data = os.read(proc.stdout.fileno(), 4096)
        assert data, ("program ended too early", bytes(output))
        output.extend(data)


try:
    observe_until(lambda text: waiting in text)
    proc.stdin.write(b"\n")
    proc.stdin.flush()
    observe_until(lambda text: text.count(waiting) >= 2
                  and b"main: notify only; ready=0\n" in text
                  and b"mutex reacquired; ready=0\n" in text)
    assert b"worker: received value=" not in output, bytes(output)
    proc.stdin.write(b"\n")
    proc.stdin.flush()
    tail, errors = proc.communicate(timeout=5)
    output.extend(tail)
    assert proc.returncode == 0 and not errors, (bytes(output), errors)
    assert b"mutex reacquired; ready=1\n" in output, bytes(output)
    assert b"worker: received value=42\n" in output, bytes(output)
    print(output.decode(), end="")
finally:
    selector.close()
    if proc.poll() is None:
        proc.kill()
        proc.communicate()

fast = subprocess.run([str(program)], input=b"\n\n", capture_output=True, timeout=5)
assert fast.returncode == 0 and not fast.stderr, (fast.stdout, fast.stderr)
assert b"worker: received value=42\n" in fast.stdout, fast.stdout

ended = subprocess.run([str(program)], input=b"", capture_output=True, timeout=5)
assert ended.returncode == 2, (ended.stdout, ended.stderr)
assert b"input ended before ENTER" in ended.stderr, ended.stderr
print("Passed: notification with false predicate re-waits; publication yields 42;")
print("fast input completes; early EOF terminates with 2.")
