# State of the Art Study: C++ TCP News Server

This document surveys the open-source C++ ecosystem for patterns, libraries, and reference implementations relevant to the task: **a TCP news retrieval server with authentication, reconnection after crash, and stream resumption from the last received message.**

Before implementing, I focused the research on the main technical questions of the exercise:

- how to structure a TCP server in C++ without relying on a large networking framework;
- how to authenticate a client before allowing it to subscribe;
- how to keep a stream ordered so a client can resume from the last message received;
- how to persist published messages so the server can recover after restart;
- how to keep recent messages available in memory for fast replay.

I first looked at repositories related to financial streaming, persistence, and networking. Some were useful as comparison points, while others directly influenced the final architecture. The most relevant references were the ones connected to non-blocking TCP servers, trading-style replay, append-only logs, binary protocols, and bounded replay buffers.

---

## 1. Initial Repositories Considered

### [MoQuant/StreamStockData](https://github.com/MoQuant/StreamStockData)

I considered this repository because it is related to real-time financial data streaming, which is close to the idea of a continuously updated news feed.

The project uses a WebSocket client connected to Alpaca's market-data stream. The relevant parts are:

- `cpprest/ws_client.h` for WebSocket communication;
- `boost::property_tree` for JSON parsing;
- API key and secret authentication;
- a subscription message for selected stock tickers;
- a continuous receive loop that parses trade updates and stores prices/volumes by ticker.

The exact technology is different from this exercise, because the requested project is a TCP server rather than a WebSocket market-data client. Still, it was useful to compare the general streaming flow:

- authenticate first;
- subscribe to a stream;
- continuously receive messages;
- decode each message;
- update in-memory state.

This helped clarify that the final project should use a custom TCP server and a simple binary protocol instead of building around an external WebSocket/JSON API.

### [onkar69483/Personal-Finance-Management-Cpp](https://github.com/onkar69483/Personal-Finance-Management-Cpp)

I considered this repository as a simple example of C++ file-based persistence.

It is a beginner-level console application using classes, `std::vector`, and `std::ofstream` / `std::ifstream` to save and load data.

The useful part was mostly comparative. It shows basic file persistence, but it does not address the main requirements of this project:

- no TCP server;
- no authentication step;
- no stream subscription;
- no replay from the last received message;
- no crash-recovery model.

The conclusion from this comparison was that simple save/load files are not enough. The server needs an append-only log pattern, where records are written as they are published and can be replayed on restart.

---

## 2. TCP Server Implementations (No Boost)

These repos demonstrate how to build a TCP server using raw OS sockets and event loops, which is exactly what we need.

| Repository | What It Does | Key Learnings |
|---|---|---|
| [Eli Bendersky's epoll-server.c](https://github.com/eliben/code-for-blog/blob/main/2017/async-socket-server/epoll-server.c) | **Best readable example** of a robust epoll event loop. Widely cited in blog posts and courses | Handling partial reads/writes, state machine for non-blocking I/O. The gold standard for learning epoll |
| [NerDante/epollServer](https://github.com/NerDante/epollServer) | High-concurrency TCP/UDP server in pure C/C++ | Clean epoll loop abstraction, pluggable message parser, connection context management |
| [shuai132/SocketPP](https://github.com/shuai132/SocketPP) | Lightweight C++11 TCP socket library, epoll (Linux) / kqueue (macOS) | Cross-platform non-blocking I/O without any framework |
| [Qihoo360/evpp](https://github.com/Qihoo360/evpp) | High-performance modern C++ networking library | Good separation of transport vs. application logic |
| [embeddedmz/socket-cpp](https://github.com/embeddedmz/socket-cpp) | Simple C++ socket wrapper with optional SSL/TLS | Clean idiomatic C++ wrapper around raw sockets |

> [!TIP]
> **For Windows (our target OS):** `epoll` is Linux-only. On Windows, the equivalent is either `select()` (simpler, good enough for a demo) or `IOCP` (production-grade). Since this is an interview exercise, **`select()` on Winsock** is perfectly acceptable and keeps the code portable.

---

## 3. Market Data Feed / Streaming Servers

These are the closest real-world analogues to our "news server." In trading, a "news feed" is essentially a "market data feed."

| Repository | What It Does | Key Learnings |
|---|---|---|
| [omerhalid/Real-Time-Market-Data-Feed-Handler](https://github.com/omerhalid/Real-Time-Market-Data-Feed-Handler-and-Order-Matching-Engine) | Feed handler + order matching engine with CPU affinity, thread pinning, lock-free SPSC queues | HFT best practices, lock-free architecture |
| [SourenaMOOSAVI/Low-Latency-Market-Data-Processor](https://github.com/SourenaMOOSAVI/Low-Latency-Market-Data-Processor) | Low-latency UDP ingestion with lock-free ring buffers | Producer-consumer pattern with ring buffers |
| [penberg/helix](https://github.com/penberg/helix) | Ultra-low-latency market data feed handler | Feed normalization, multi-source aggregation |

---

## 4. Crash Recovery & Persistence (WAL / Append-Only Logs)

This is how we solve the "reprise du flux après crash" requirement. The industry pattern is a **Write-Ahead Log (WAL)**.

| Repository | What It Does | Key Learnings |
|---|---|---|
| [google/leveldb](https://github.com/google/leveldb) | **Gold standard** for C++ log-structured storage + WAL | WAL record format: `[length | data | CRC32]`. Recovery by replaying log. Checkpointing to truncate |
| [facebook/rocksdb](https://github.com/facebook/rocksdb) | Production-grade C++ WAL, fork of LevelDB | Extensive WAL implementation, group commit, fsync strategies |
| [MengRao/tcpshm](https://github.com/MengRao/tcpshm) | **⭐ Highly relevant** — Persistent message queue via shared memory + TCP | Combines TCP transport with persistent message storage. Closest to what we're building |
| [vimpunk/mio](https://github.com/vimpunk/mio) | Cross-platform header-only C++ library for memory-mapped file I/O | Foundation for building mmap-based persistence |

> [!IMPORTANT]
> **The WAL Pattern (What We Should Implement)**
> 
> Every news message is written to an append-only file on disk before being broadcast:
> ```
> Record = [ 4-byte length | 8-byte sequence_id | payload bytes | 4-byte CRC32 ]
> ```
> On server startup after a crash, replay the file from the beginning to rebuild the in-memory state and recover the last sequence number. This is exactly what LevelDB and RocksDB do internally.

---

## 5. Sequence Numbers & Message Replay (Stream Resumption)

This is how we solve "reprise du flux depuis le dernier message reçu." The FIX protocol (the standard protocol for trading) has solved this problem for 30+ years.

| Repository | What It Does | Key Learnings |
|---|---|---|
| [quickfix/quickfix](https://github.com/quickfix/quickfix) | **Industry standard** FIX protocol engine | **Most directly relevant**: sequence tracking, persistent message store, ResendRequest, Gap Fill. This is exactly our stream resumption pattern |
| [aidancbrady/openfix](https://github.com/aidancbrady/openfix) | Modern C++23 FIX engine | Modern C++ approach to message replay, automatic recovery via ResendRequest |
| [fix8/fix8](https://github.com/fix8/fix8) | High-performance schema-driven FIX framework | Mature session layer with sequence management |

> [!IMPORTANT]
> **The FIX Protocol Replay Pattern (What We Should Implement)**
> 
> 1. Every message gets a monotonically increasing **Sequence Number**
> 2. The client tracks its `last_received_seq`
> 3. On reconnect, the client sends: "I last saw sequence #42"
> 4. The server looks in its persistent store, retrieves messages #43 through #N, and replays them
> 5. Once caught up, the client receives the live stream
> 
> This is standard practice in every trading system in the world.

---

## 6. Ring Buffers (In-Memory Streaming)

For keeping recent messages in fast memory so that reconnecting clients can be served without hitting disk.

| Repository | What It Does | Key Learnings |
|---|---|---|
| [rigtorp/SPSCQueue](https://github.com/rigtorp/SPSCQueue) | **Top choice** — Header-only, wait-free single-producer single-consumer queue | Cache-line alignment (`alignas(64)`), acquire/release memory ordering, power-of-2 sizing |
| [cameron314/readerwriterqueue](https://github.com/cameron314/readerwriterqueue) | Production-ready SPSC queue | Battle-tested alternative to SPSCQueue |
| [DNedic/lockfree](https://github.com/DNedic/lockfree) | Collection of C++11 lock-free structures | Simpler implementations, good for reference and learning |

> [!TIP]
> **Ring Buffer Tips for Low-Latency**
> - Use power-of-2 sizes (bitwise AND `& (size-1)` instead of `% size`)
> - `alignas(64)` on head and tail indices to prevent false sharing between CPU cache lines
> - Use `std::memory_order_acquire` / `release` for atomic operations (not `seq_cst`)

---

## 7. Binary Protocol Design

For structuring the TCP packets exchanged between client and server.

| Repository | What It Does | Key Learnings |
|---|---|---|
| [commschamp/comms](https://github.com/commschamp/comms) | C++11 header-only library for custom binary protocols. No exceptions, no RTTI, minimal allocation | Protocol definition DSL, transport framing, message dispatching. Excellent reference |
| [penberg/libtrading](https://github.com/penberg/libtrading) | FIX, FIX/FAST, ITCH/OUCH protocol implementations | Financial binary protocol parsing patterns |

> [!NOTE]
> **Binary Framing Pattern**
> 
> The standard way to frame messages over TCP (which is a byte stream, not a message stream):
> ```
> [ 4-byte message length (big-endian) ][ message payload ]
> ```
> The receiver reads 4 bytes → knows exactly how many more bytes to read → parses the payload. This avoids delimiter-based parsing (like newline-delimited JSON) which is slower and more fragile.

---

## 8. Authentication Over TCP

| Repository | What It Does | Key Learnings |
|---|---|---|
| [nilshenrich/TCP_ServerClient](https://github.com/nilshenrich/TCP_ServerClient) | TCP client/server wrapper with TLS 1.3 + mutual authentication | TLS integration, certificate-based auth |
| [ASK-03/SSH-Server-Client-Project](https://github.com/ASK-03/SSH-Server-Client-Project) | Basic username/password over TCP | Simple auth flow |

> [!NOTE]
> **For our scope:** A simple username + password sent as the first message after TCP connect is sufficient for a demo. For extra points in the interview, we could implement a **challenge-response** pattern:
> 1. Client sends username
> 2. Server sends a random nonce
> 3. Client computes `HMAC(password, nonce)` and sends it back
> 4. Server verifies — no plaintext password ever crosses the wire

---

## 9. Full Architecture References (Trading Systems)

| Repository | What It Does | Key Learnings |
|---|---|---|
| [eelixir/mercury](https://github.com/eelixir/mercury) | Full matching engine with nanosecond instrumentation | End-to-end trading architecture reference |
| [ranjan2829/High-Frequency-Trading-Exchange-Engine](https://github.com/ranjan2829/High-Frequency-Trading-Exchange-Engine) | Lock-free SPSC queues, memory pools, CPU affinity, non-blocking TCP | Comprehensive HFT technique showcase |
| [enewhuis/liquibook](https://github.com/enewhuis/liquibook) | Mature open-source order matching engine | Clean, modular C++ design |

---

## 10. Summary: Recommended Architecture for Our Project

Based on this study, here is the architecture that would be most impressive in a trading interview:

```
┌─────────────────────────────────────────────────────────┐
│                      TCP SERVER                         │
│                                                         │
│  ┌──────────┐    ┌──────────────┐    ┌───────────────┐  │
│  │  Socket   │───▶│   Session    │───▶│  Auth Check   │  │
│  │  Accept   │    │  (per client)│    │  (in-memory   │  │
│  │  Loop     │    │              │    │   hash map)   │  │
│  └──────────┘    └──────┬───────┘    └───────────────┘  │
│                         │                               │
│                         ▼                               │
│              ┌─────────────────────┐                    │
│              │   News Generator    │                    │
│              │   (background       │                    │
│              │    thread/timer)     │                    │
│              └─────────┬───────────┘                    │
│                        │                                │
│              ┌─────────▼───────────┐                    │
│              │   Append-Only Log   │  ◀── crash safe    │
│              │   (binary file)     │                    │
│              └─────────┬───────────┘                    │
│                        │                                │
│              ┌─────────▼───────────┐                    │
│              │   Ring Buffer       │  ◀── fast replay   │
│              │   (in-memory)       │                    │
│              └─────────────────────┘                    │
│                                                         │
│  On reconnect: client sends last_seq_id                 │
│  → server replays from ring buffer or log file          │
└─────────────────────────────────────────────────────────┘
```

### Key Technology Choices

| Component | Approach | Why |
|---|---|---|
| **Networking** | Raw sockets (`select` or `epoll`) | Shows system-level knowledge; no framework dependency |
| **Persistence** | Append-only binary log file | Fast, crash-safe, industry standard (LevelDB/RocksDB pattern) |
| **Fast replay** | In-memory ring buffer | Avoids disk I/O for recent reconnections |
| **Protocol** | Length-prefixed binary frames | Standard in trading; faster than JSON parsing |
| **Auth** | Username/password or challenge-response | Simple but demonstrates the handshake concept |
| **Resumption** | Sequence numbers + replay (FIX pattern) | Proven by 30+ years of trading infrastructure |
| **Build** | CMake | Industry standard for C++ |

### Top 5 Repos to Study Before Implementing

1. **[MengRao/tcpshm](https://github.com/MengRao/tcpshm)** — Persistent message queue over TCP. Closest to what we're building.
2. **[quickfix/quickfix](https://github.com/quickfix/quickfix)** — The exact sequence number / replay / gap fill pattern we need.
3. **[google/leveldb](https://github.com/google/leveldb)** — Gold standard WAL implementation for crash recovery.
4. **[Eli Bendersky's epoll-server](https://github.com/eliben/code-for-blog/blob/main/2017/async-socket-server/epoll-server.c)** — Best readable example of a non-blocking socket server.
5. **[rigtorp/SPSCQueue](https://github.com/rigtorp/SPSCQueue)** — Reference ring buffer for the streaming pipeline.
