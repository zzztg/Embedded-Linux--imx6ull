import pathlib
import re
import subprocess
import sys

def captured_count(stderr):
    match = re.search(rb"parent: EOF, captured (\d+) bytes", stderr)
    assert match, stderr
    return int(match.group(1))

def check_args(stdout, stderr):
    parent_pid = re.search(rb"parent before fork: pid=(\d+)", stderr).group(1)
    child_pid = re.search(rb"child setup: pid=(\d+)", stderr).group(1)
    expected_first = b"new program: pid=" + child_pid + b", ppid=" + parent_pid + b", argc=3\n"
    assert stdout.startswith(expected_first), stdout
    assert b"argv[0]=args_demo\nargv[1]=hello\nargv[2]=i.MX6ULL\n" in stdout
    assert stdout.endswith(b"new program: returning 7\n"), stdout
    assert b"parent:" not in stdout and b"stderr:" not in stdout
    assert b"new program stderr: pid=" + child_pid in stderr
    assert b"parent: child exit code=7" in stderr
    assert captured_count(stderr) == len(stdout)

normal = subprocess.run(
    ["./pipe_exec_host", "./args_demo_host"], capture_output=True, timeout=10
)
assert normal.returncode == 0, normal.stderr
check_args(normal.stdout, normal.stderr)
print("NORMAL captured stdout\n" + normal.stdout.decode(), end="")
print("NORMAL stderr\n" + normal.stderr.decode(), end="")

replay = pathlib.Path("replay.txt")
with replay.open("wb") as output:
    saved = subprocess.run(
        ["./pipe_exec_host", "./args_demo_host"], stdout=output,
        stderr=subprocess.PIPE, timeout=10,
    )
assert saved.returncode == 0, saved.stderr
check_args(replay.read_bytes(), saved.stderr)
print(f"REPLAY FILE: {replay.stat().st_size} bytes, exactly matches parent count")

missing = subprocess.run(
    ["./pipe_exec_host", "./missing_exec_parent_for_demo/args_demo"],
    capture_output=True, timeout=10,
)
assert missing.returncode == 0 and missing.stdout == b"", missing.stderr
assert b"child execl:" in missing.stderr
assert b"parent: child exit code=127" in missing.stderr
assert captured_count(missing.stderr) == 0
print("MISSING TARGET: EOF with 0 bytes, child reaped with status 127")

bulk = pathlib.Path("bulk_writer.py")
bulk.write_text(
    "#!" + sys.executable + "\nimport sys\n"
    "sys.stdout.buffer.write(b'0123456789ABCDEF' * 65536)\n"
    "sys.stdout.buffer.flush()\nsys.exit(7)\n"
)
bulk.chmod(0o755)
large = subprocess.run(
    ["./pipe_exec_host", "./bulk_writer.py"], capture_output=True, timeout=10
)
assert large.returncode == 0, large.stderr
assert large.stdout == b"0123456789ABCDEF" * 65536
assert captured_count(large.stderr) == 1048576
assert b"parent: child exit code=7" in large.stderr
print("BULK: all 1048576 bytes captured, EOF and child reaping completed")
