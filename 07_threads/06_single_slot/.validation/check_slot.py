from pathlib import Path
import re
import subprocess

program = Path(__file__).resolve().parent / "project" / "slot_demo_host"
run = subprocess.run([str(program)], capture_output=True, text=True, timeout=5)
assert run.returncode == 0 and not run.stderr, (run.stdout, run.stderr)

occupied = None
puts = []
takes = []
for line in run.stdout.splitlines():
    put = re.fullmatch(r"main: put value=(\d+); full=1", line)
    take = re.fullmatch(r"worker: take value=(\d+); full=0", line)
    if put:
        assert occupied is None, ("overwrote an unconsumed item", line, run.stdout)
        occupied = int(put[1])
        puts.append(occupied)
    elif take:
        item = int(take[1])
        assert occupied == item, ("took from empty or wrong slot", line, run.stdout)
        occupied = None
        takes.append(item)

assert puts == takes == [10, 20, 30], run.stdout
assert occupied is None, run.stdout
assert "main: joined; count=3, sum=60, expected_sum=60\n" in run.stdout, run.stdout
assert run.stdout.endswith("main: synchronization objects destroyed\n"), run.stdout
print(run.stdout, end="")
print("Passed: one-slot occupancy invariant, each item taken once in order,")
print("count=3, sum=60, clean termination; no required scheduling pattern.")
