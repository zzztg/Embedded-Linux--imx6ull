import pathlib
import re
import subprocess

def run(target, output):
    result = subprocess.run(
        ["./exec_redirect_host", target, str(output)],
        capture_output=True, text=True, timeout=5,
    )
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert "stdout still at original destination" in result.stdout
    assert "parent: reaped pid=" in result.stdout
    return result

output = pathlib.Path("child_stdout.txt")
output.write_text("old content must be truncated\n")
success = run("./args_demo_host", output)
contents = output.read_text()
parent_pid = re.search(r"parent before fork: pid=(\d+)", success.stdout).group(1)
child_pid = re.search(r"parent: child_pid=(\d+)", success.stdout).group(1)
assert f"new program: pid={child_pid}, ppid={parent_pid}, argc=3\n" in contents
assert "argv[0]=args_demo\nargv[1]=hello\nargv[2]=i.MX6ULL\n" in contents
assert contents.endswith("new program: returning 7\n")
assert "parent" not in contents and "stderr" not in contents and "old content" not in contents
assert "new program:" not in success.stdout
assert f"child setup: pid={child_pid}" in success.stderr
assert f"new program stderr: pid={child_pid}" in success.stderr
assert "parent: child exit code=7" in success.stdout
print("SUCCESS console stdout\n" + success.stdout, end="")
print("SUCCESS console stderr\n" + success.stderr, end="")
print("SUCCESS output file\n" + contents, end="")

assert not pathlib.Path("missing_exec_parent_for_demo").exists()
failed_output = pathlib.Path("exec_failed.txt")
failed_output.write_text("stale output\n")
missing = run("./missing_exec_parent_for_demo/args_demo", failed_output)
assert "child execl:" in missing.stderr
assert "new program stderr:" not in missing.stderr
assert "parent: child exit code=127" in missing.stdout
assert failed_output.read_bytes() == b""
print("MISSING PROGRAM: child exit 127; file exists but empty; parent exit 0")

assert not pathlib.Path("missing_output_parent_for_demo").exists()
bad_output = run("./args_demo_host", "./missing_output_parent_for_demo/out.txt")
assert "child open output:" in bad_output.stderr
assert "new program stderr:" not in bad_output.stderr
assert "parent: child exit code=126" in bad_output.stdout
print("MISSING OUTPUT DIRECTORY: child exit 126; target did not run; parent exit 0")
