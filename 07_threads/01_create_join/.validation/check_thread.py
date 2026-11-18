import re
import subprocess

result = subprocess.run(["./thread_demo_host"], capture_output=True, text=True, timeout=5)
assert result.returncode == 0 and not result.stderr, (result.stdout, result.stderr)
match = re.fullmatch(
    r"main: pid=(\d+), before create number=10\n"
    r"worker: pid=\1, initial number=10\n"
    r"worker: changed number=99; returning\n"
    r"main: after join number=99\n", result.stdout,
)
assert match, result.stdout
print(result.stdout, end="")
print("Passed: same process PID, valid shared object, worker completion before final read, exit 0")
