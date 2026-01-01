# Multi-threaded Web Proxy Server with LRU Cache

A multi-threaded HTTP proxy server written in C++ that efficiently handles concurrent client connections. It features a Least Recently Used (LRU) cache to store and serve frequently accessed web pages, significantly reducing latency and bandwidth usage.

## Features

- **Multi-threading:** Utilizes POSIX threads (`pthread`) to handle multiple client connections simultaneously, ensuring responsiveness even under load.
- **LRU Cache:** Implements a custom LRU cache using a combination of a Doubly Linked List and a Hash Map (`std::unordered_map`) for $O(1)$ access and updates.
- **Thread Safety:** Uses Mutex locks (`pthread_mutex`) to ensure thread-safe access to the shared cache and Semaphores (`sem_t`) to manage client connection limits.
- **HTTP Protocol Support:** Parses and forwards HTTP GET requests.
- **Error Handling:** Generates appropriate HTTP error responses (400, 403, 404, 500, 501, 505).
- **Connection Management:** Handles client-server communication and gracefully manages connection closures.

## Requirements

- GCC/G++ Compiler
- Make
- POSIX Threads library (`pthread`)

## Build Instructions

To compile the project, simply run:

```bash
make
```

This will generate the `proxy` executable.

## Usage

Start the proxy server by specifying the port number:

```bash
./proxy <port_number>
```

**Example:**

```bash
./proxy 8080
```

Once running, configure your web browser or HTTP client to use `localhost` (or the server's IP) and the specified port as the HTTP proxy.

## Implementation Details

- **`server_LRU_cache.cpp`**: Contains the main server logic, thread handling, and LRU cache implementation.
- **`proxy_parse.cpp` / `proxy_parse.h`**: Helper library for parsing HTTP requests.
- **`Makefile`**: Build script for compiling the project.

### LRU Cache Design
The cache is implemented using:
1.  **Doubly Linked List:** Stores the cache elements (web pages) ordered by usage (most recently used at the head).
2.  **Hash Map:** Maps URLs to the corresponding nodes in the linked list for constant-time lookup.

When the cache is full, the least recently used item (at the tail of the list) is removed to make space for new content.

## Cleaning Up

To remove compiled object files and the executable:

```bash
make clean
```
