import pathlib
import re
import subprocess

input_path = pathlib.Path("input.txt")
original = b"ABCDEFGHIJKLMNOP"
input_path.write_bytes(original)
for mode, expected_parent_before, expected_parent_data in (
    ("shared", 4, "EFGH"), ("separate", 0, "ABCD")
):
    result = subprocess.run(
        ["./fd_inherit_host", mode, str(input_path)],
        capture_output=True, text=True, timeout=5,
    )
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert not result.stderr, result.stderr
    child = re.search(r'child: fd=(\d+), offset=0 -> 4, read=4, data="ABCD"',
                      result.stdout)
    assert child, result.stdout
    after = expected_parent_before + 4
    parent = re.search(
        rf'parent: fd=(\d+), offset={expected_parent_before} -> {after}, '
        rf'read=4, data="{expected_parent_data}"', result.stdout,
    )
    assert parent, result.stdout
    assert "child: closed its fd; local fd=-1" in result.stdout
    if mode == "shared":
        assert f"parent: child reaped; local fd={child.group(1)}" in result.stdout
    else:
        assert "parent: child reaped; local fd=-1" in result.stdout
    assert input_path.read_bytes() == original
    print(result.stdout, end="")

print("Both modes passed: offsets match expected sharing; input unchanged")
