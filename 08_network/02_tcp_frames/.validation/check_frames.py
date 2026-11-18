from pathlib import Path
import selectors
import socket
import struct
import subprocess

project = Path(__file__).resolve().parent / "project"
server_program = project / "frame_server_host"
client_program = project / "build" / "host" / "frame_client"


def start_server():
    proc = subprocess.Popen([str(server_program)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as selector:
            selector.register(proc.stdout, selectors.EVENT_READ)
            assert selector.select(5), "server not ready"
            first = proc.stdout.readline()
        if not first.startswith(b"server: listening on 0.0.0.0:8889;"):
            out, err = proc.communicate(timeout=5)
            raise AssertionError((first + out, err))
        return proc, first
    except BaseException:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()
        raise


def finish(proc, first, code):
    tail, errors = proc.communicate(timeout=5)
    output = first + tail
    assert proc.returncode == code, (output, errors)
    assert b"listener closed; connection_fd=" in output, output
    return output, errors


def stop(proc):
    if proc.poll() is None:
        proc.kill()
        proc.communicate()


def receive_exact(peer, length):
    data = bytearray()
    while len(data) < length:
        part = peer.recv(length - len(data))
        assert part, ("incomplete response", length, len(data))
        data.extend(part)
    return bytes(data)


server, first = start_server()
try:
    client = subprocess.run([str(client_program), "127.0.0.1"], capture_output=True, timeout=5)
    assert client.returncode == 0, (client.stdout, client.stderr)
    assert b'frame=1, length=5, matches=yes, text="HELLO"' in client.stdout
    assert b'frame=2, length=8, matches=yes, text="i.MX6ULL"' in client.stdout
    output, errors = finish(server, first, 0)
    assert b"clean EOF; frames=2, payload_bytes=13" in output, output
    assert b"progress=4/4" in errors and b"progress=4/4" in client.stderr
    print(client.stdout.decode(), end="")
    print(output.decode(), end="")
finally:
    stop(server)

payloads = [b"", b"A\x00B", bytes(range(64))]
server, first = start_server()
try:
    wire = b"".join(struct.pack("!I", len(p)) + p for p in payloads)
    with socket.create_connection(("127.0.0.1", 8889), timeout=3) as peer:
        peer.sendall(wire)
        peer.shutdown(socket.SHUT_WR)
        for payload in payloads:
            length = struct.unpack("!I", receive_exact(peer, 4))[0]
            assert length == len(payload)
            assert receive_exact(peer, length) == payload
        assert peer.recv(1) == b""
    output, errors = finish(server, first, 0)
    assert b"clean EOF; frames=3, payload_bytes=67" in output, output
    print("Passed: combined frames, empty frame, embedded NUL, maximum payload and network order")
finally:
    stop(server)

cases = [
    (b"", 0, b"clean EOF; frames=0, payload_bytes=0"),
    (b"\x00\x00", 1, b"EOF inside field"),
    (struct.pack("!I", 5), 1, b"EOF before the declared payload"),
    (struct.pack("!I", 5) + b"AB", 1, b"EOF inside field"),
    (struct.pack("!I", 65), 1, b"payload length 65 exceeds limit"),
]
for wire, expected_code, marker in cases:
    server, first = start_server()
    try:
        with socket.create_connection(("127.0.0.1", 8889), timeout=3) as peer:
            peer.sendall(wire)
            peer.shutdown(socket.SHUT_WR)
            assert peer.recv(1) == b""
        output, errors = finish(server, first, expected_code)
        assert marker in output + errors, (output, errors)
        if expected_code:
            assert b"frame=1," not in output, output
    finally:
        stop(server)
print("Passed: clean boundary EOF, truncated header/body, missing body, oversize rejection")
