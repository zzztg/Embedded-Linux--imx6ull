from pathlib import Path
import os
import re
import selectors
import subprocess
import time

program = Path(__file__).resolve().parent / "project" / "broadcast_demo_host"
for args, expected in [((), [10, 0, 30]), (("empty",), [])]:
    proc = subprocess.Popen([str(program), *args], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output = bytearray()
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ)
    try:
        deadline = time.monotonic() + 5
        while not all(f"worker {i}: empty and open; waiting\n".encode() in output
                      for i in (1, 2)):
            remaining = deadline - time.monotonic()
            assert remaining > 0 and selector.select(remaining), bytes(output)
            data = os.read(proc.stdout.fileno(), 4096)
            assert data, bytes(output)
            output.extend(data)
        proc.stdin.write(b"\n")
        proc.stdin.flush()
        tail, errors = proc.communicate(timeout=5)
        output.extend(tail)
        assert proc.returncode == 0 and not errors, (bytes(output), errors)
    finally:
        selector.close()
        if proc.poll() is None:
            proc.kill()
            proc.communicate()

    text = output.decode()
    occupied = None
    takes = []
    per_worker = {1: [], 2: []}
    for line in text.splitlines():
        put = re.fullmatch(r"main: put value=(\d+); full=1", line)
        take = re.fullmatch(r"worker ([12]): take value=(\d+); full=0", line)
        if put:
            assert occupied is None, text
            occupied = int(put[1])
        elif take:
            assert occupied == int(take[2]), text
            takes.append(occupied)
            per_worker[int(take[1])].append(occupied)
            occupied = None
    assert takes == expected and occupied is None, text
    for i in (1, 2):
        assert text.count(f"worker {i}: closed and empty; exiting\n") == 1, text
        assert f"main: joined worker {i}; count={len(per_worker[i])}, sum={sum(per_worker[i])}\n" in text, text
    assert f"main: total count={len(expected)}, sum={sum(expected)}, expected_count={len(expected)}, expected_sum={sum(expected)}\n" in text, text
    assert text.endswith("main: synchronization objects destroyed\n"), text
    print(text, end="")
print("Passed: two initial waiters, exactly-once ordered takes, per-worker statistics,")
print("both exits and joins after close in normal and empty modes, exit 0.")
