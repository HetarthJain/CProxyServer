import socket
import time

PROXY_HOST = "127.0.0.1"
PROXY_PORT = 8080

TARGETS = [
    "example.com",
    # "neverssl.com",
    "httpforever.com",
    "info.cern.ch",
]

REQUEST_PATH = "/"
TIMEOUT = 5


def recv_all(sock):
    data = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
    return data


def direct_request(host):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((host, 80))

    request = f"GET {REQUEST_PATH} HTTP/1.0\r\nHost: {host}\r\n\r\n"

    start = time.time()
    s.sendall(request.encode())
    response = recv_all(s)
    elapsed = time.time() - start
    s.close()

    return response, elapsed


def proxy_request(host):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((PROXY_HOST, PROXY_PORT))

    request = f"GET http://{host}{REQUEST_PATH} HTTP/1.0\r\n\r\n"

    start = time.time()
    s.sendall(request.encode())
    response = recv_all(s)
    elapsed = time.time() - start
    s.close()

    return response, elapsed


def status_line(resp):
    if not resp:
        return "NO RESPONSE"
    return resp.split(b"\r\n", 1)[0].decode(errors="ignore")


def test_target(host):
    print(f"\n🌐 Testing {host}")

    direct_resp, direct_time = direct_request(host)
    proxy_resp, proxy_time = proxy_request(host)

    print(
        f"  Direct → {status_line(direct_resp)} | {len(direct_resp)} bytes | {direct_time:.3f}s"
    )
    print(
        f"  Proxy  → {status_line(proxy_resp)} | {len(proxy_resp)} bytes | {proxy_time:.3f}s"
    )
    # print("direct resp-\n", direct_resp)
    # print("proxy resp-\n", proxy_resp)
    if direct_resp != proxy_resp:
        print("  ⚠️  Response mismatch")
    else:
        print("  ✅ Responses identical")


def main():
    for host in TARGETS:
        test_target(host)


if __name__ == "__main__":
    main()
