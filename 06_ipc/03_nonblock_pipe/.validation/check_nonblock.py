import re
import subprocess

result = subprocess.run(
    ["./nonblock_demo_host"], capture_output=True, text=True, timeout=3
)
assert result.returncode == 0, (result.stdout, result.stderr)
assert not result.stderr, result.stderr
output = result.stdout
assert "read nonblocking=yes, write nonblocking=no" in output
assert re.search(r"1 empty, writer open: read=-1, would block .*errno=\d+", output)
assert '2 first data: read=2, data="AB"' in output
assert '3 remaining data: read=2, data="CD"' in output
assert re.search(r"4 empty again, writer open: read=-1, would block .*errno=\d+", output)
assert "5 empty, no writers: read=0 (EOF)" in output
assert output.count("read=-1") == 2
assert output.count("(EOF)") == 1
print(output, end="")
print("Passed: read-only nonblocking flag, two EAGAIN results, intact data, final EOF, exit 0")
