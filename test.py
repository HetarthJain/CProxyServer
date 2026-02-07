import socket
import threading
import time

PROXY_HOST = "127.0.0.1"
PROXY_PORT = 8080       # change if needed
TARGET_HOST = "example.com"
REQUEST_PATH = "/"
NUM_CLIENTS = 50        # increase to stress harder
TIMEOUT = 5


def send_request(client_id):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((PROXY_HOST, PROXY_PORT))

        request = (
            f"GET http://{TARGET_HOST}{REQUEST_PATH} HTTP/1.1\r\n"
            f"Host: {TARGET_HOST}\r\n"
            "Connection: close\r\n"
            "\r\n"
        )

        s.sendall(request.encode())

        response = b""
        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data

        print(f"[Client {client_id}] Received {len(response)} bytes")

        if b"HTTP/" not in response:
            print(f"[Client {client_id}] ⚠️ Invalid HTTP response")

        s.close()

    except Exception as e:
        print(f"[Client {client_id}] ❌ Error: {e}")


def main():
    threads = []
    start = time.time()

    for i in range(NUM_CLIENTS):
        t = threading.Thread(target=send_request, args=(i,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print(f"\nDone. Total time: {time.time() - start:.2f}s")


if __name__ == "__main__":
    main()
