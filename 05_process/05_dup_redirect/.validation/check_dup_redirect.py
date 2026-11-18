import pathlib
import re
import subprocess

input_path = pathlib.Path("input.txt")
original = b"ABCDEFGHIJKLMNOP"
input_path.write_bytes(original)
duplicated = subprocess.run(
    ["./dup_demo_host", str(input_path)], capture_output=True, text=True, timeout=5
)
assert duplicated.returncode == 0, (duplicated.stdout, duplicated.stderr)
assert not duplicated.stderr, duplicated.stderr
descriptors = re.search(r"original fd=(\d+), duplicate fd=(\d+)", duplicated.stdout)
assert descriptors and descriptors.group(1) != descriptors.group(2)
assert re.search(r'original: fd=\d+, offset=0 -> 4, read=4, data="ABCD"', duplicated.stdout)
assert re.search(r'duplicate: fd=\d+, offset=4 -> 8, read=4, data="EFGH"', duplicated.stdout)
assert re.search(r'duplicate after close: fd=\d+, offset=8 -> 12, read=4, data="IJKL"', duplicated.stdout)
assert input_path.read_bytes() == original
print("DUP CASE\n" + duplicated.stdout, end="")

output_path = pathlib.Path("redirected.txt")
output_path.write_text("old contents must be truncated\n")
redirected = subprocess.run(
    ["./redirect_demo_host", str(output_path)], capture_output=True, text=True, timeout=5
)
assert redirected.returncode == 0, (redirected.stdout, redirected.stderr)
assert redirected.stdout == (
    "stdout: before redirection\n"
    "stdout: restored to original destination\n"
), redirected.stdout
assert "setup: file_fd=" in redirected.stderr, redirected.stderr
assert "stderr: still at original destination\n" in redirected.stderr
assert output_path.read_bytes() == b"stdout: this line goes to the file.\n"
print("REDIRECT CASE stdout\n" + redirected.stdout, end="")
print("REDIRECT CASE stderr\n" + redirected.stderr, end="")
print("REDIRECT CASE file\n" + output_path.read_text(), end="")
print("Passed: shared offsets, surviving duplicate, exact file output, stdout restored, stderr unchanged")
