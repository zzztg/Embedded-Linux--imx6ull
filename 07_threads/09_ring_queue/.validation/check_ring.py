from collections import deque
from pathlib import Path
import re
import subprocess

program = Path(__file__).resolve().parent / "project" / "ring_demo_host"
for args, expected in [((), [10, 0, 30, 40, 50, 60]), (("empty",), [])]:
    run = subprocess.run([str(program), *args], capture_output=True, text=True, timeout=5)
    assert run.returncode == 0 and not run.stderr, (run.stdout, run.stderr)
    head = tail = count = 0
    closed = False
    queue = deque()
    puts, takes = [], []
    per_worker = {1: [], 2: []}
    for line in run.stdout.splitlines():
        put = re.fullmatch(r"main: put value=(\d+) at=(\d+); head=(\d+) tail=(\d+) count=(\d+)", line)
        take = re.fullmatch(r"worker ([12]): take value=(\d+) at=(\d+); head=(\d+) tail=(\d+) count=(\d+)", line)
        closing = re.fullmatch(r"queue: closed=1; remaining=(\d+); broadcasting", line)
        if put:
            value, index, logged_head, logged_tail, logged_count = map(int, put.groups())
            assert not closed and count < 3 and index == tail, run.stdout
            tail = (tail + 1) % 3
            count += 1
            queue.append(value)
            puts.append(value)
            assert (head, tail, count) == (logged_head, logged_tail, logged_count), run.stdout
        elif take:
            worker, value, index, logged_head, logged_tail, logged_count = map(int, take.groups())
            assert count > 0 and index == head and queue.popleft() == value, run.stdout
            head = (head + 1) % 3
            count -= 1
            takes.append(value)
            per_worker[worker].append(value)
            assert (head, tail, count) == (logged_head, logged_tail, logged_count), run.stdout
        elif closing:
            assert int(closing[1]) == count, run.stdout
            closed = True
        elif "closed and empty; exiting" in line:
            assert closed and count == 0, run.stdout
    assert puts == takes == expected and not queue and count == 0 and closed, run.stdout
    for worker in (1, 2):
        assert run.stdout.count(f"worker {worker}: closed and empty; exiting\n") == 1, run.stdout
        assert f"main: joined worker {worker}; count={len(per_worker[worker])}, sum={sum(per_worker[worker])}\n" in run.stdout, run.stdout
    assert f"main: total count={len(expected)}, sum={sum(expected)}, expected_count={len(expected)}, expected_sum={sum(expected)}\n" in run.stdout, run.stdout
    if expected:
        assert "main: before starting workers; head=0 tail=0 count=3\n" in run.stdout
    assert run.stdout.endswith("main: synchronization objects destroyed\n"), run.stdout
    print(run.stdout, end="")
print("Passed: every index/count transition, FIFO and exactly-once delivery,")
print("normal/empty close, per-worker statistics, both joins, exit 0.")
