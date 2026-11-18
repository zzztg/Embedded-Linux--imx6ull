import re
import subprocess

result = subprocess.run(["./backpressure_demo_host"],
                        capture_output=True, text=True, timeout=5)
assert result.returncode == 0 and not result.stderr, (result.stdout, result.stderr)
text = result.stdout
assert "1 initially empty: poll=1, POLLOUT=yes" in text
written = re.search(r"accepted_total=(\d+) bytes", text)
drained = re.search(r"drained_total=(\d+) bytes", text)
assert written and drained and int(written.group(1)) > 0, text
assert written.group(1) == drained.group(1), text
assert "3 after write EAGAIN: poll=0, POLLOUT=no" in text
assert "5 after draining: poll=1, POLLOUT=yes" in text
assert "6 retry write: returned 2" in text
assert '7 verification read: returned 2, data="OK"' in text
print(text, end="")
print("Passed: bounded fill, EAGAIN, matching drained bytes, writable again, retry payload intact")
