from pathlib import Path
import selectors
import socket
import subprocess

project = Path(__file__).resolve().parent / "project"
server_program = project / "tcp_server_host"
client_program = project / "build" / "host" / "tcp_client"


def start_server():
    proc = subprocess.Popen([str(server_program)], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as selector:
            selector.register(proc.stdout, selectors.EVENT_READ)
            assert selector.select(5), "server did not report readiness"
            first = proc.stdout.readline()
        if not first.startswith(b"server: listening on 0.0.0.0:8888;"):
            out, err = proc.communicate(timeout=5)
            raise AssertionError((first + out, err))
        return proc, first
    except BaseException:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()
        raise


def finish_server(proc, first, expected_count):
    tail, errors = proc.communicate(timeout=5)
    output = first + tail
    assert proc.returncode == 0 and not errors, (output, errors)
    assert f"echoed {expected_count} bytes\n".encode() in output, output
    assert b"accepted one connection; connection_fd=" in output, output
    assert output.endswith(b"server: sockets closed; exiting\n"), output
    print(output.decode(), end="")


server, first = start_server()
try:
    client = subprocess.run([str(client_program), "127.0.0.1"],
                            capture_output=True, timeout=5)
    assert client.returncode == 0 and not client.stderr, (client.stdout, client.stderr)
    assert b"client: sent 16 bytes\n" in client.stdout, client.stdout
    assert b"Hello from WSL!\n" in client.stdout, client.stdout
    assert b"client: echo matches: yes\n" in client.stdout, client.stdout
    print(client.stdout.decode(), end="")
    finish_server(server, first, 16)
finally:
    if server.poll() is None:
        server.kill()
        server.communicate()

for payload in (bytes(range(256)) * 2, b""):
    server, first = start_server()
    try:
        with socket.create_connection(("127.0.0.1", 8888), timeout=3) as peer:
            peer.sendall(payload)
            peer.shutdown(socket.SHUT_WR)
            echoed = bytearray()
            while True:
                chunk = peer.recv(1024)
                if not chunk:
                    break
                echoed.extend(chunk)
        assert bytes(echoed) == payload, (len(echoed), len(payload))
        finish_server(server, first, len(payload))
    finally:
        if server.poll() is None:
            server.kill()
            server.communicate()

invalid = subprocess.run([str(client_program), "not-an-ip"], capture_output=True, timeout=5)
assert invalid.returncode == 2 and b"invalid IPv4 address" in invalid.stderr
print("Passed: native C client/server echo, 512-byte binary and empty input,")
print("EOF and cleanup, invalid IPv4 handling; board run remains pending.")
