import socket
import threading
import time

PROXY_HOST = "127.0.0.1"
PROXY_PORT = 8080

TARGETS = [
    "example.com",
    # "neverssl.com",
    "httpforever.com",
    "info.cern.ch",
    "cs.princeton.edu",
]

REQUEST_PATH = "/"
NUM_CLIENTS = 5
TIMEOUT = 30


def send_request(client_id, target_host):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((PROXY_HOST, PROXY_PORT))

        request = f"GET http://{target_host}{REQUEST_PATH} HTTP/1.0\r\n\r\n"

        s.sendall(request.encode())

        response = b""
        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data

        if not response:
            print(f"[Client {client_id}] ❌ Empty response")
            return

        # Extract status line
        status_line = response.split(b"\r\n", 1)[0]

        print(
            f"[Client {client_id}] {target_host} | "
            f"{status_line.decode(errors='ignore')} | "
            f"{len(response)} bytes"
        )

        if not status_line.startswith(b"HTTP/"):
            print(f"[Client {client_id}] ⚠️ Invalid HTTP response")

        s.close()

    except Exception as e:
        print(f"[Client {client_id}] ❌ Error: {e}")


def main():
    threads = []
    start = time.time()

    for i in range(NUM_CLIENTS):
        target = TARGETS[i % len(TARGETS)]
        t = threading.Thread(target=send_request, args=(i, target))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print(f"\nDone. Total time: {time.time() - start:.2f}s")


if __name__ == "__main__":
    main()
