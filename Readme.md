# C Multithreaded HTTP Proxy Server

## Overview

This project implements a **multithreaded HTTP proxy server in C** with support for **HTTP/1.0 and HTTP/1.1**, response parsing, and an in-memory **thread-safe cache** with an approximate Least Recently Used (LRU) eviction policy.

The proxy accepts client connections, forwards requests to origin servers, relays responses back to clients, and caches eligible responses to improve performance on repeated requests.

This project focuses on **systems-level correctness**: socket lifecycle management, concurrency, HTTP framing, and memory safety.

---

## Features

* Multithreaded server using a **thread pool + job queue**
* Supports **HTTP/1.0 and HTTP/1.1** client requests
* Correct handling of:

  * Content-Length responses
  * Chunked transfer encoding
* Response header parsing (custom implementation)
* Thread-safe in-memory cache
* Timestamp-based **approximate LRU eviction**
* Per-bucket reader–writer locks for scalable concurrency
* Graceful handling of malformed requests and server errors

---

## Architecture

### High-level flow

```
Client
  ↓
Proxy (accept)
  ↓
Job Queue
  ↓
Worker Thread
  ↓
Origin Server
  ↓
Proxy (response parsing + caching)
  ↓
Client
```

### Threading model

* Main thread:

  * Accepts incoming client connections
  * Enqueues client sockets into a bounded job queue

* Worker threads:

  * Dequeue client sockets
  * Parse client requests
  * Forward requests to origin servers
  * Parse and relay responses
  * Update cache
  * Close client connections

---

## HTTP Handling

### Supported request formats

* Absolute-form requests (required for proxies):

```
GET http://example.com/path HTTP/1.1
```

### Request parsing

* Implemented in `parse.c / parse.h`
* Extracts:

  * Method
  * Host
  * Port
  * Path
  * HTTP version
  * Headers

### Response parsing

* Implemented in `response_parse.c / response_parse.h`
* Parses **response headers only**
* Extracts:

  * HTTP version
  * Status code
  * Header length
  * Content-Length (if present)
  * Transfer-Encoding: chunked (if present)

### Response body handling

* HTTP/1.0 or no framing → read until server closes
* HTTP/1.1 + Content-Length → read exact number of bytes
* HTTP/1.1 + chunked → forward chunks until final `0\r\n\r\n`

---

## Cache Design

### What the cache does

* Stores full HTTP responses keyed by request URL
* Serves cached responses directly to clients on cache hits
* Significantly reduces latency on repeated requests

### Cache structure

* Hash-based buckets
* Each bucket contains a linked list of cache entries
* Each bucket protected by a reader–writer lock

### Cache entry

* URL (key)
* Response data (deep copy)
* Response length
* Last-access timestamp

### Eviction policy

* Timestamp-based **approximate LRU**
* When a bucket exceeds its size limit:
  * The entry with the oldest access timestamp is evicted

### Important notes

* This is **not a strict O(1) LRU** implementation
* Eviction is O(n) within a bucket
---

## Correctness Guarantees

* No shared mutable response buffers
* No use-after-free in cache access
* Client sockets always closed after one request
* Origin server sockets closed deterministically
* No reliance on server-side connection close for HTTP/1.1 framing

---

## Build Instructions

```
gcc -Wall -Wextra -pthread *.c -o proxy
```

---

## Running the Proxy

```
./proxy <PORT>
```

Example:

```
./proxy 8080
```

---

## Testing

### Python test client

A Python script (`test.py`) is provided to simulate multiple concurrent clients.

Features:

* Direct vs proxy comparison
* HTTP/1.0 requests
* Concurrent connections using Python threads

Example:

```
python3 test.py
```

Expected behavior:

* First run: slower (cold cache)
* Subsequent runs: significantly faster (cache hits)

---

## Limitations

* No HTTPS tunneling (CONNECT method not implemented)
* No HTTP/1.1 keep-alive support
* Cache eviction is approximate LRU, not strict LRU
* No persistent disk-backed cache

---

## Possible Extensions

* True O(1) LRU cache using doubly linked lists
* Global cache eviction across buckets
* HTTPS proxying via CONNECT
* HTTP/1.1 persistent connections
* Non-blocking I/O with epoll
* Metrics (hit rate, eviction count, latency)

