import os
import pathlib
import re
import selectors
import signal
import subprocess
import sys
import time

def command(target, prefix):
    return ["./poll_capture_host", target, prefix + "_out.txt", prefix + "_err.txt"]

def check_totals(log, prefix):
    for name, suffix in (("stdout", "out"), ("stderr", "err")):
        match = re.search(rf"parent: {name} EOF, total=(\d+),", log)
        assert match, log
        assert int(match.group(1)) == pathlib.Path(prefix + "_" + suffix + ".txt").stat().st_size
    assert "parent: both channels reached EOF" in log
    assert "parent: reaped pid=" in log

normal = subprocess.run(command("./stream_writer_host", "normal"),
                        capture_output=True, text=True, timeout=10)
assert normal.returncode == 0, (normal.stdout, normal.stderr)
child_pid = re.search(r"parent: pid=\d+, child_pid=(\d+)", normal.stdout).group(1)
assert pathlib.Path("normal_out.txt").read_text() == (
    f"stdout: pid={child_pid}\nstdout: work complete\n")
assert pathlib.Path("normal_err.txt").read_text() == (
    f"stderr: pid={child_pid}\nstderr: diagnostic complete\n")
check_totals(normal.stdout, "normal")
assert "parent: child exit code=7" in normal.stdout
print("NORMAL\n" + normal.stdout, end="")

missing = subprocess.run(command("./missing_exec_parent_for_demo/stream_writer", "missing"),
                         capture_output=True, text=True, timeout=10)
assert missing.returncode == 0, (missing.stdout, missing.stderr)
assert pathlib.Path("missing_out.txt").read_bytes() == b""
assert "child execl:" in pathlib.Path("missing_err.txt").read_text()
assert "parent: child exit code=127" in missing.stdout
check_totals(missing.stdout, "missing")
print("MISSING TARGET: stdout empty, exec error captured in stderr, both channels reaped")

bulk = pathlib.Path("bulk_writer.py")
bulk.write_text("#!" + sys.executable + "\nimport os, sys\n"
                "sys.stderr.buffer.write(b'ERR\\n' * 65536)\n"
                "sys.stderr.flush()\nsys.stderr.close()\nos.close(2)\n"
                "sys.stdout.buffer.write(b'OUT\\n' * 65536)\n"
                "sys.stdout.flush()\nsys.exit(7)\n")
bulk.chmod(0o755)
large = subprocess.run(command("./bulk_writer.py", "bulk"),
                       capture_output=True, text=True, timeout=10)
assert large.returncode == 0, (large.stdout[-1000:], large.stderr)
assert pathlib.Path("bulk_out.txt").read_bytes() == b"OUT\n" * 65536
assert pathlib.Path("bulk_err.txt").read_bytes() == b"ERR\n" * 65536
check_totals(large.stdout, "bulk")
print("BULK: 262144 stderr bytes first, then 262144 stdout bytes, no loss or deadlock")

early = pathlib.Path("early_close.py")
early.write_text("#!" + sys.executable + "\nimport os, sys\n"
                 "sys.stdout.write('early stdout\\n')\nsys.stdout.close()\nos.close(1)\n"
                 "sys.stdin.buffer.readline()\n"
                 "sys.stderr.write('later stderr\\n')\nsys.stderr.flush()\nsys.exit(7)\n")
early.chmod(0o755)
proc = subprocess.Popen(command("./early_close.py", "early"), stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)
prefix = b""
try:
    deadline = time.monotonic() + 5
    while not re.search(rb"stdout EOF, total=\d+, active channels=1", prefix):
        assert time.monotonic() < deadline, prefix
        for key, _ in selector.select(timeout=0.1):
            chunk = os.read(key.fd, 4096)
            assert chunk, prefix
            prefix += chunk
    assert proc.poll() is None
    assert b"both channels reached EOF" not in prefix
    assert pathlib.Path("early_err.txt").stat().st_size == 0
    rest, errors = proc.communicate(b"\n", timeout=5)
    complete = (prefix + rest).decode()
    assert proc.returncode == 0, (complete, errors)
    assert pathlib.Path("early_out.txt").read_text() == "early stdout\n"
    assert pathlib.Path("early_err.txt").read_text() == "later stderr\n"
    check_totals(complete, "early")
    print("EARLY EOF: stdout removed while stderr remained active; later stderr captured")
finally:
    selector.close()
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate(timeout=5)
