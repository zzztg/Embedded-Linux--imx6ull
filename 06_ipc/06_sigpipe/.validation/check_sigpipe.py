import errno
import re
import signal
import subprocess

def run(mode):
    return subprocess.run(["./sigpipe_demo_host", mode],
                          capture_output=True, text=True, timeout=5)

default = run("default")
assert default.returncode == -signal.SIGPIPE, (default.returncode, default.stdout, default.stderr)
assert "all read ends closed; writing 1 byte" in default.stdout
assert "after write" not in default.stdout
print("DEFAULT: terminated by SIGPIPE; after-write code did not run")

for mode, expected_caught in (("ignore", 0), ("catch", int(signal.SIGPIPE))):
    result = run(mode)
    assert result.returncode == 1 and not result.stderr, (result.stdout, result.stderr)
    assert "after write: returned -1" in result.stdout
    assert re.search(rf"saved errno={errno.EPIPE} \(", result.stdout)
    assert f"caught_signal={expected_caught}\n" in result.stdout
    assert "EPIPE: no readers; stop sending on this pipe" in result.stdout
    print(mode.upper() + "\n" + result.stdout, end="")

shell = subprocess.run(
    ["bash", "-c", './sigpipe_demo_host default; study_status=$?; printf "shell_status=%d\\n" "$study_status"'],
    capture_output=True, text=True, timeout=5,
)
assert f"shell_status={128 + int(signal.SIGPIPE)}" in shell.stdout, (shell.stdout, shell.stderr)
print(f"BASH: shell reports {128 + int(signal.SIGPIPE)} for signal termination")
