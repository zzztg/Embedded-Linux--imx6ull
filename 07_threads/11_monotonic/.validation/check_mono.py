from pathlib import Path
import subprocess
import time

program = Path(__file__).resolve().parent / "project" / "mono_demo_host"
prefix = "main: condition initialized with CLOCK_MONOTONIC; attributes destroyed\n"
start = time.monotonic()
run = subprocess.run([str(program)], capture_output=True, text=True, timeout=7)
elapsed = time.monotonic() - start
assert run.returncode == 0 and not run.stderr, (run.stdout, run.stderr)
assert elapsed >= 2.5, ("possible deadline/condition clock mismatch", elapsed, run.stdout)
assert run.stdout.startswith(prefix), run.stdout
assert "worker: ETIMEDOUT; mutex reacquired; ready=0\n" in run.stdout
assert "main: joined; outcome=timeout\n" in run.stdout
assert run.stdout.endswith("main: condition variable and mutex destroyed\n")
print(run.stdout, end="")
print(f"Monotonic wait elapsed: {elapsed:.3f} seconds")

run = subprocess.run([str(program), "ready"], capture_output=True, text=True, timeout=5)
assert run.returncode == 0 and not run.stderr, (run.stdout, run.stderr)
assert run.stdout.startswith(prefix), run.stdout
assert "waiting with one monotonic" not in run.stdout and "ETIMEDOUT" not in run.stdout
assert "main: joined; outcome=data, value=42\n" in run.stdout
assert run.stdout.endswith("main: condition variable and mutex destroyed\n")
print(run.stdout, end="")
print("Passed: configured monotonic deadline, attributes destroyed before worker use,")
print("timeout and prepared paths, joins and condition cleanup, exit 0.")
