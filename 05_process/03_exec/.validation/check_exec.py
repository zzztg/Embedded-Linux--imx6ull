import pathlib
import re
import subprocess

def run(path):
    result = subprocess.run(
        ["./exec_demo_host", path], capture_output=True, text=True, timeout=5
    )
    assert result.returncode == 0, (result.returncode, result.stderr)
    return result

success = run("./args_demo_host")
before = re.search(r"child before exec: pid=(\d+), ppid=(\d+)", success.stdout)
after = re.search(r"new program: pid=(\d+), ppid=(\d+), argc=3", success.stdout)
assert before and after, success.stdout
assert before.groups() == after.groups(), success.stdout
assert f"reaped pid={before.group(1)}" in success.stdout, success.stdout
assert "argv[0]=args_demo\nargv[1]=hello\nargv[2]=i.MX6ULL\n" in success.stdout
assert "parent: child exit code=7" in success.stdout, success.stdout
assert "execl:" not in success.stderr, success.stderr
print("SUCCESS CASE\n" + success.stdout, end="")

missing_path = "./missing_exec_parent_for_demo/args_demo"
assert not pathlib.Path(missing_path).exists()
failure = run(missing_path)
assert "child before exec:" in failure.stdout, failure.stdout
assert "new program:" not in failure.stdout, failure.stdout
assert "parent: child exit code=127" in failure.stdout, failure.stdout
assert "execl:" in failure.stderr, failure.stderr
print("MISSING PROGRAM CASE\n" + failure.stdout + failure.stderr, end="")
print("Both cases: child reaped; launcher exit code=0")
