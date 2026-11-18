from pathlib import Path
import os
import selectors
import socket
import subprocess
import time

project = Path(__file__).resolve().parent / "project"
server_program = project / "half_server_host"
client_program = project / "build" / "host" / "half_client"


def observe(proc, marker):
    output = bytearray()
    deadline = time.monotonic() + 5
    with selectors.DefaultSelector() as selector:
        selector.register(proc.stdout, selectors.EVENT_READ)
        while marker not in output:
            remaining = deadline - time.monotonic()
            assert remaining > 0 and selector.select(remaining), bytes(output)
            data = os.read(proc.stdout.fileno(), 4096)
            assert data, ("early stdout EOF", bytes(output))
            output.extend(data)
    return bytes(output)


def start_server():
    proc = subprocess.Popen([str(server_program)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        first = observe(proc, b"server: listening on 0.0.0.0:8890\n")
        return proc, first
    except BaseException:
        stop(proc)
        raise


def stop(proc):
    if proc.poll() is None:
        proc.kill()
        proc.communicate()


def finish(proc, prefix, length):
    tail, errors = proc.communicate(timeout=5)
    text = prefix + tail
    assert proc.returncode == 0 and not errors, (text, errors)
    eof = f"server: EOF; total={length}\n".encode()
    reply = f"server: request ended; sending {length}-byte reply AFTER EOF\n".encode()
    assert eof in text and reply in text and text.index(eof) < text.index(reply), text
    assert text.endswith(b"server: connection closed; exiting\n"), text
    return text


server, first = start_server()
try:
    client = subprocess.run([str(client_program), "127.0.0.1"], capture_output=True, timeout=5)
    assert client.returncode == 0 and not client.stderr, (client.stdout, client.stderr)
    assert b"write side shut down; fd remains open for reading" in client.stdout
    assert b"client: EOF; total=16\n" in client.stdout
    assert b"client: reply matches=yes" in client.stdout
    print(client.stdout.decode(), end="")
    print(finish(server, first, 16).decode(), end="")
finally:
    stop(server)

server, first = start_server()
client = subprocess.Popen([str(client_program), "127.0.0.1", "hold"], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
try:
    before_client = observe(client, b"press ENTER to shutdown SHUT_WR\n")
    before_server = observe(server, b"total=16\n")
    assert b"server: EOF" not in before_server and b"sending" not in before_server
    assert client.poll() is None and server.poll() is None
    client.stdin.write(b"\n")
    client.stdin.flush()
    tail, errors = client.communicate(timeout=5)
    output = before_client + tail
    assert client.returncode == 0 and not errors, (output, errors)
    assert b"client: reply matches=yes" in output
    finish(server, first + before_server, 16)
    print("Passed: hold receives 16 bytes but no EOF/reply until ENTER and SHUT_WR")
finally:
    stop(client)
    stop(server)

for payload in (b"", bytes(range(64))):
    server, first = start_server()
    try:
        with socket.create_connection(("127.0.0.1", 8890), timeout=3) as peer:
            peer.sendall(payload)
            peer.shutdown(socket.SHUT_WR)
            echoed = bytearray()
            while True:
                part = peer.recv(128)
                if not part:
                    break
                echoed.extend(part)
        assert bytes(echoed) == payload
        finish(server, first, len(payload))
    finally:
        stop(server)

server, first = start_server()
try:
    with socket.create_connection(("127.0.0.1", 8890), timeout=3) as peer:
        peer.sendall(b"X" * 65)
        try:
            peer.shutdown(socket.SHUT_WR)
            assert peer.recv(1) == b""
        except (ConnectionResetError, BrokenPipeError):
            pass
    out, errors = server.communicate(timeout=5)
    assert server.returncode == 1 and b"data exceeds 64-byte limit" in errors, (first + out, errors)
    assert b"reply AFTER EOF" not in first + out
finally:
    stop(server)
print("Passed: empty and maximum binary requests, response after peer EOF, capacity rejection")
