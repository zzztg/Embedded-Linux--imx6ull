import pathlib
import re
import subprocess
import time

proc = subprocess.Popen(
    ["./zombie_demo_host"], stdin=subprocess.PIPE,
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
)
startup = []
child_pid = None
try:
    for _ in range(5):
        line = proc.stdout.readline()
        startup.append(line)
        match = re.search(r"child_pid=(\d+)", line)
        if match:
            child_pid = int(match.group(1))
        if "press ENTER" in line:
            break
    assert child_pid is not None, startup
    status_path = pathlib.Path(f"/proc/{child_pid}/status")
    deadline = time.monotonic() + 3
    while True:
        state = next(line for line in status_path.read_text().splitlines()
                     if line.startswith("State:"))
        if "Z (zombie)" in state:
            break
        assert time.monotonic() < deadline, state
        time.sleep(0.02)
    print(f"before reaping: child={child_pid}, {state}")
    assert proc.poll() is None, "parent must still be alive"
    output, errors = proc.communicate("\n", timeout=5)
    print("".join(startup) + output, end="")
    assert proc.returncode == 0, errors
    assert f"reaped pid={child_pid}" in output, output
    assert "child exit code=7" in output, output
    assert not status_path.exists(), "child proc entry still present"
    print("after reaping: child proc entry absent; parent exit code=0")
finally:
    if proc.poll() is None:
        proc.kill()
        proc.communicate(timeout=5)
