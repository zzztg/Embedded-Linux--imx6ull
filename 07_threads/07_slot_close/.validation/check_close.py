from pathlib import Path
import re
import subprocess

program = Path(__file__).resolve().parent / "project" / "close_demo_host"
for args, expected in [((), [10, 0, 30]), (("empty",), [])]:
    run = subprocess.run([str(program), *args], capture_output=True, text=True, timeout=5)
    assert run.returncode == 0 and not run.stderr, (run.stdout, run.stderr)
    occupied = None
    closed = False
    puts, takes = [], []
    for line in run.stdout.splitlines():
        put = re.fullmatch(r"main: put value=(-?\d+); full=1", line)
        take = re.fullmatch(r"worker: take value=(-?\d+); full=0", line)
        closing = re.fullmatch(r"queue: closed=1; full=([01])", line)
        if put:
            assert occupied is None and not closed, run.stdout
            occupied = int(put[1])
            puts.append(occupied)
        elif take:
            assert occupied == int(take[1]), run.stdout
            takes.append(occupied)
            occupied = None
        elif closing:
            assert int(closing[1]) == int(occupied is not None), run.stdout
            closed = True
        elif line == "worker: closed and empty; end of stream":
            assert closed and occupied is None, run.stdout
    assert puts == takes == expected and occupied is None and closed, run.stdout
    assert f"count={len(expected)}, sum={sum(expected)}, expected_count={len(expected)}, expected_sum={sum(expected)}" in run.stdout
    assert run.stdout.endswith("main: synchronization objects destroyed\n"), run.stdout
    print(run.stdout, end="")
print("Passed: order, no overwrite, zero data, drain before EOF, empty close, exit 0")
