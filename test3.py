import socket
import threading
import time
import random
import statistics

PROXY_HOST = "127.0.0.1"
PROXY_PORT = 8080

# Mix of cacheable + redirect + larger pages
TARGETS = [
    "example.com",
    "httpforever.com",
    "info.cern.ch",
    # "neverssl.com",
    "cs.princeton.edu",
]

REQUEST_PATH = "/"

NUM_CLIENTS = 500  # total requests
CONCURRENCY = 50  # active clients at once
TIMEOUT = 10


lock = threading.Lock()
latencies = []
errors = 0
completed = 0


def send_request(target):
    global errors, completed

    start = time.time()
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((PROXY_HOST, PROXY_PORT))

        req = f"GET http://{target}{REQUEST_PATH} HTTP/1.0\r\n\r\n"
        s.sendall(req.encode())
		
        while True:
            data = s.recv(4096)
            if not data:
                break

        s.close()

        latency = time.time() - start
        with lock:
            latencies.append(latency)
            completed += 1

    except Exception:
        with lock:
            errors += 1

def send_request1(target):
    global errors, completed

    start = time.time()
    response = b""

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((PROXY_HOST, PROXY_PORT))

        req = f"GET http://{target}{REQUEST_PATH} HTTP/1.0\r\n\r\n"
        s.sendall(req.encode())

        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data

        s.close()

        latency = time.time() - start

        with lock:
            latencies.append(latency)

            if response.startswith(b"HTTP/1.1 503") or response.startswith(
                b"HTTP/1.0 503"
            ):
                # Overload rejection = success
                completed += 1
            elif response.startswith(b"HTTP/"):
                # Normal HTTP response
                completed += 1
            else:
                # Malformed response
                errors += 1

    except Exception:
        with lock:
            errors += 1



def worker_thread(requests):
    for target in requests:
        send_request(target)


def main():
    print(f"🔥 Stress testing proxy on {PROXY_HOST}:{PROXY_PORT}")
    print(f"Clients: {NUM_CLIENTS}, Concurrency: {CONCURRENCY}\n")

    # Prepare request list (forces cache reuse)
    targets = [random.choice(TARGETS) for _ in range(NUM_CLIENTS)]

    start = time.time()
    threads = []

    for i in range(0, NUM_CLIENTS, CONCURRENCY):
        batch = targets[i : i + CONCURRENCY]
        t = threading.Thread(target=worker_thread, args=(batch,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    total_time = time.time() - start

    print("\n===== RESULTS =====")
    print(f"Total requests: {NUM_CLIENTS}")
    print(f"Completed: {completed}")
    print(f"Errors: {errors}")
    print(f"Total time: {total_time:.2f}s")

    if latencies:
        print(f"Avg latency: {statistics.mean(latencies):.3f}s")
        print(f"95th percentile: {statistics.quantiles(latencies, n=20)[18]:.3f}s")
        print(f"Max latency: {max(latencies):.3f}s")

    print("===================")


if __name__ == "__main__":
    main()
