from pathlib import Path
import stat
import subprocess
import tempfile

program = Path(__file__).resolve().parent / "project" / "fifo_demo_host"
message = b"Hello through FIFO!\n"
with tempfile.TemporaryDirectory(prefix="imx6ull-fifo-") as temp:
    fifo = Path(temp) / "study.fifo"
    made = subprocess.run([str(program), "create", str(fifo)], capture_output=True, timeout=5)
    assert made.returncode == 0 and stat.S_ISFIFO(fifo.stat().st_mode), (made.stdout, made.stderr)
    inode = fifo.stat().st_ino
    duplicate = subprocess.run([str(program), "create", str(fifo)], capture_output=True, timeout=5)
    assert duplicate.returncode == 1 and fifo.stat().st_ino == inode

    for first_role, second_role in [("read", "write"), ("write", "read")]:
        processes = []
        try:
            first = subprocess.Popen([str(program), first_role, str(fifo)],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(first)
            try:
                first.communicate(timeout=0.2)
                raise AssertionError("single-ended blocking open unexpectedly completed")
            except subprocess.TimeoutExpired:
                pass
            second = subprocess.Popen([str(program), second_role, str(fifo)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(second)
            second_out, second_err = second.communicate(timeout=5)
            first_out, first_err = first.communicate(timeout=5)
            assert first.returncode == second.returncode == 0, (first_err, second_err)
            reader_out, reader_err = ((first_out, first_err) if first_role == "read"
                                      else (second_out, second_err))
            writer_out, writer_err = ((first_out, first_err) if first_role == "write"
                                      else (second_out, second_err))
            assert reader_out == message and b"EOF; received 20 bytes" in reader_err
            assert not writer_out and b"sent 20 bytes" in writer_err
            assert stat.S_ISFIFO(fifo.stat().st_mode) and fifo.stat().st_ino == inode
            print(f"Passed: {first_role} opens first, peer connects, 20 bytes, EOF, both exit 0")
        finally:
            for proc in processes:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()

    ordinary = Path(temp) / "ordinary.txt"
    ordinary.write_bytes(b"keep this file unchanged")
    for role in ("read", "write", "create"):
        result = subprocess.run([str(program), role, str(ordinary)], capture_output=True, timeout=5)
        assert result.returncode == 1 and ordinary.read_bytes() == b"keep this file unchanged"
    print("Passed: FIFO node reused across connections; existing paths not replaced;")
    print("ordinary files rejected before transfer and left unchanged.")
