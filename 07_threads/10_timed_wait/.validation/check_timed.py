from pathlib import Path
import subprocess
import time

program = Path(__file__).resolve().parent / "project" / "timed_demo_host"
start = time.monotonic()
timeout = subprocess.run([str(program)], capture_output=True, text=True, timeout=7)
elapsed = time.monotonic() - start
assert timeout.returncode == 0 and not timeout.stderr, (timeout.stdout, timeout.stderr)
assert elapsed >= 2.5, (elapsed, timeout.stdout)
assert "worker: ETIMEDOUT; mutex reacquired; ready=0\n" in timeout.stdout
assert "main: joined; outcome=timeout\n" in timeout.stdout
assert timeout.stdout.endswith("main: synchronization objects destroyed\n")
print(timeout.stdout, end="")
print(f"Timeout path elapsed: {elapsed:.3f} seconds")

prepared = subprocess.run([str(program), "ready"], capture_output=True, text=True, timeout=5)
assert prepared.returncode == 0 and not prepared.stderr, (prepared.stdout, prepared.stderr)
assert "timed wait" not in prepared.stdout and "ETIMEDOUT" not in prepared.stdout
assert "main: joined; outcome=data, value=42\n" in prepared.stdout
assert prepared.stdout.endswith("main: synchronization objects destroyed\n")
print(prepared.stdout, end="")
print("Passed: timeout handled normally; prepared state skips wait; joins and cleanup")
