# Implementation Plan: Low-Latency C++ TCP News Server

## 1. Objective

Build an interview-ready C++ news streaming system that runs on Linux/WSL and demonstrates:

- authenticated TCP client sessions;
- non-blocking networking with raw POSIX sockets and `epoll`;
- a length-prefixed binary protocol;
- monotonically increasing news ids;
- crash recovery through an append-only write-ahead log (WAL);
- stream resumption from the last news id received by a client;
- fast replay of recent messages from an in-memory replay ring;
- bounded memory use and explicit slow-client handling.

The implementation will not use Boost, a networking framework, or a database.

## 2. Scope and Success Criteria

The first complete version is successful when it can:

1. Start, recover all valid WAL records, and continue with the next news id.
2. Accept multiple non-blocking TCP clients through one `epoll` event loop.
3. Authenticate a client before allowing subscription or replay.
4. Publish each news item only after it has been appended to the WAL.
5. Deliver live news in id order.
6. Reconnect a client using its last received id and replay every later item.
7. Serve recent replay from memory and older replay from the WAL.
8. Disconnect a client whose output queue exceeds a configured bound.
9. Build and pass automated tests under WSL using CMake and CTest.

TLS, distributed replication, user administration, and production-grade secret storage are explicitly outside the initial scope.

## 3. Proposed Architecture

### 3.1 Runtime components

- **Main/event-loop thread**: owns the listening socket, all client sockets, session state, protocol parsing, replay scheduling, and live broadcast.
- **News source**: a simple publisher thread generates deterministic demo titles every 20 seconds and wakes the event loop through an `eventfd`.
- **WAL**: one append-only binary file. The event loop appends a complete record before making that news item visible to clients.
- **Replay ring**: a fixed-capacity circular array holding the latest decoded news records.
- **Signal handling**: a `signalfd` registered with `epoll` enables orderly shutdown without asynchronous signal-handler logic.

The event loop owns sockets, sessions, ids, WAL appends, replay, and broadcast. The publisher thread only generates titles and notifies the server, keeping shared state small and easy to explain.

### 3.2 Data flow

```text
publisher thread
        |
        v
generate title -> eventfd wakes server event loop
        |
        v
assign id -> encode WAL record -> append to WAL
        |                                      |
        | append succeeds                      | append fails
        v                                      v
insert into replay ring -> broadcast       stop publishing,
to authenticated live clients              report fatal error

reconnecting client -> authenticate -> provide last_seen_id
                                      |
                     +----------------+----------------+
                     |                                 |
                id in ring                       id too old
                     |                                 |
              replay from memory                 scan/read WAL
                     +----------------+----------------+
                                      |
                              join live stream
```

## 4. Repository Layout

```text
News/
|-- CMakeLists.txt
|-- README.md
|-- config/
|   `-- users.conf.example
|-- include/news/
|   |-- protocol.h
|   |-- byte_buffer.h
|   |-- session.h
|   |-- epoll_server.h
|   |-- wal.h
|   |-- replay_ring.h
|   `-- news_record.h
|-- src/
|   |-- binary_encoding.h
|   |-- protocol.cc
|   |-- session.cc
|   |-- epoll_server.cc
|   |-- news_publisher.cc
|   |-- wal.cc
|   |-- replay_ring.cc
|   `-- server_main.cc
|-- client/
|   |-- news_client.h
|   |-- news_client.cc
|   `-- client_main.cc
|-- tests/
|   |-- protocol_test.cc
|   |-- authenticator_test.cc
|   |-- wal_test.cc
|   |-- replay_ring_test.cc
|   `-- integration_test.cc
```

The sample client is part of the deliverable: it proves authentication, id tracking, reconnection, and replay without relying on external tools.

## 5. Binary Protocol

### 5.1 Transport frame

TCP is a byte stream, so every application message uses an explicit frame:

```text
FrameHeader (network byte order)
  uint32 body_length
  uint16 message_type
Body[body_length]
```

Rules:

- The header has a fixed encoded size; do not transmit C++ structs directly.
- Integers are explicitly encoded/decoded in big-endian order.
- `body_length` excludes the frame header and is capped at 64 KiB.
- The demo protocol assumes valid message formats and keeps error handling intentionally simple.
- The client receive loop supports multiple frames in one `recv()` call.

### 5.2 Initial message types

| Type | Direction | Purpose |
|---|---|---|
| `AUTH_REQUEST` | Client to server | Username and password for the exercise |
| `AUTH_RESULT` | Server to client | Authentication success or failure |
| `SUBSCRIBE` | Client to server | Contains `last_seen_id` |
| `NEWS` | Server to client | News id and title |

`NEWS` payload:

```text
uint64 id
uint32 title_length
byte[] title
```

The protocol uses the word `payload` in code, but conceptually this is the frame body.

### 5.3 Session state machine

```text
CONNECTED -> AUTHENTICATED -> LIVE -> CLOSING
     |             |              |
     +-------------+--------------+--> CLOSING on error
```

- Only `AUTH_REQUEST` is legal in `CONNECTED`.
- Only `SUBSCRIBE` is legal in `AUTHENTICATED`.
- Authentication uses a fixed configuration file for the exercise. Plaintext credentials are acceptable only on the local demo network; the README must state that production deployment requires TLS or challenge-response authentication and hashed secrets.

## 6. Persistence and Crash Recovery

### 6.1 WAL file format

Each record is written in a compact binary format:

```text
Record:
  uint64 id
  uint32 title_length
  title bytes
```

Example for id `5` and title `"Hi"`:

```text
[00 00 00 00 00 00 00 05][00 00 00 02][48 69]
```

WAL values use the same documented big-endian byte order as the protocol and are serialized field by field.

### 6.2 Append policy

- Open the WAL in binary append mode.
- Fully encode a bounded record before writing it.
- Never broadcast a record if its append fails.

The current version skips CRC/checksum and advanced durability modes to keep the implementation small and interview-explainable.

### 6.3 Startup recovery

On startup:

1. Read records sequentially.
2. Stop at the first incomplete record.
3. Add recovered records to the replay ring, naturally retaining only the newest capacity.
4. Set `next_id = last_valid_id + 1`.

Older replay is implemented by scanning the WAL and returning records from the requested id.

## 7. Replay Ring and Resumption

Use a fixed-capacity vector of recent records. Because the event loop is the sole owner, it does not need atomics or a concurrent queue.

The ring exposes:

- `append(NewsRecord)`;
- `oldest_id()`;
- `From(first_id)`;

On `SUBSCRIBE(last_seen_id)`:

1. If `last_seen_id` is greater than the newest server id, treat the client as caught up.
2. Otherwise compute `first_required = last_seen_id + 1`.
3. If `first_required` is in the ring, enqueue records from memory.
4. Otherwise, read records from the WAL.
5. Then mark the client as live.

The replay path is intentionally simple and sends the missed records before live delivery.

## 8. Networking and Backpressure

### 8.1 Socket setup

- Create non-blocking, close-on-exec sockets (`SOCK_NONBLOCK | SOCK_CLOEXEC`).
- Set `SO_REUSEADDR` and enable `TCP_NODELAY` by default.
- Use `accept4()` until it returns `EAGAIN`.
- Register the listener, clients, publisher `eventfd`, and `signalfd` with `epoll`.
- Start with level-triggered `epoll` for simpler correctness. Consider edge-triggered mode only after tests and measurements justify it.
- Ignore `SIGPIPE` or send with `MSG_NOSIGNAL`.

### 8.2 Per-session buffers

Each session owns:

- a bounded receive buffer and incremental frame-parser state;
- authentication and subscription state;
- a FIFO transmit queue of immutable encoded frames or buffer slices;
- an offset into the front frame for partial `send()` completion;

Enable `EPOLLOUT` only when a session has pending bytes, and disable it when the queue drains.

### 8.3 Slow-client policy

Memory must remain bounded. Configure a maximum queued byte count per session. If adding another frame would exceed it:

- close the client session;
- log the reason.

The initial policy is disconnect rather than dropping news, because silently creating an id gap violates the protocol. The client can reconnect and resume later.

## 9. Implementation Milestones

### Milestone 0: Build skeleton

- Add CMake targets for a core library, server, client, and tests.
- Enable C++20 and compiler warnings.
- Add a small configuration object for port, WAL path, ring capacity, and buffer limits.
- Add a README with WSL build/run commands.

**Exit condition:** clean build and one smoke test through CTest.

### Milestone 1: Protocol codec and parser

- Implement endian-safe integer helpers.
- Implement frame encoding without transmitting native structs.
- Implement simple frame encoding/decoding and frame-size helpers.
- Add tests for round trips and multiple frames in one buffer.

**Exit condition:** protocol tests pass.

### Milestone 2: WAL and recovery

- Implement file/record encoding, append, sequential recovery, and replay from a requested id.
- Add tests for append/recover and reading records from a given id.

**Exit condition:** WAL tests pass and recovered ids continue from the last record.

### Milestone 3: Replay ring

- Implement fixed-capacity storage and simple oldest-record eviction.
- Add boundary tests for empty, partially full, full, and evicted ranges.

**Exit condition:** requested retained records are returned once and in order.

### Milestone 4: Non-blocking server and authentication

- Implement listener creation, the `epoll` loop, accept/read/write draining, session cleanup, and signal-driven shutdown.
- Implement the session state machine and fixed-file credential loading.

**Exit condition:** multiple clients can authenticate and subscribe concurrently.

### Milestone 5: Publishing and live delivery

- Add a publisher thread that generates deterministic news titles every 20 seconds.
- Assign ids, append to WAL, update the ring, and broadcast in that exact order.
- Complete the sample client so it displays its last received id.

**Exit condition:** all connected clients observe the same gap-free id stream.

### Milestone 6: Replay and reconnect

- Implement memory replay, WAL replay, and high-water-mark handling.
- Add reconnect support to the client.
- Test disconnect/reconnect inside ring coverage and beyond ring coverage.

**Exit condition:** a client reconnects after missing messages and receives each missing id exactly once before live delivery.

### Milestone 7: Fault handling and integration tests

- Enforce all receive, transmit, and replay limits.
- Test abrupt disconnects, invalid credentials, stale/future resume values, WAL replay, server restart, and clean shutdown.

**Exit condition:** integration tests prove bounded memory, ordered recovery, and continued service to healthy clients.

### Milestone 8: Measurement-driven optimization

- Add latency histograms for append-to-send and replay throughput.
- Preallocate/reuse buffers and encoded live frames where profiling shows allocation pressure.
- Consider `writev()`/`sendmsg()` batching.
- Consider CPU affinity and edge-triggered `epoll` only after correctness remains covered by tests.

**Exit condition:** benchmark results and architecture trade-offs are documented; optimizations are supported by measurements.

## 10. Test Strategy

### Unit tests

- Protocol encode/decode and frame-size helpers.
- WAL serialization, append, recovery, and replay from id.
- Ring wraparound and range retrieval.
- Session read/write behavior.

### Integration tests

- Start the server on an ephemeral port and connect real TCP clients.
- Authenticate, subscribe from zero, and validate ordered news.
- Disconnect, allow publication to continue, reconnect, and validate replay.
- Restart the server with the same WAL and validate id continuation.
- Keep one client from reading while another remains healthy.
- Send frames one byte at a time and several frames in one write.

### Tooling

- Debug builds: `-Wall -Wextra -Wpedantic -Wconversion`.
- Race checking can be added later with ThreadSanitizer if the concurrency model grows.
- System-call inspection: `strace`.
- CPU and allocation profiling: `perf` and an appropriate heap profiler if needed.
- Network robustness: loopback tests plus optional `tc netem` latency/loss testing.

## 11. Operational and Security Notes

- Validate configuration before opening the listening socket.
- Apply restrictive permissions to the WAL and credential file.
- Never log passwords or raw authentication payloads.
- Log connection ID, replay range, disconnect reason, and last delivered id.
- Use structured, rate-limited logs outside the send/receive hot path where practical.
- Exit on unrecoverable WAL corruption or append failure rather than serving data that is not recoverable.
- Document maximum frame size, ring capacity, and output-queue limit.

## 12. Interview Demonstration

The final demonstration should be short and observable:

1. Build and run all tests.
2. Start the server and two authenticated clients.
3. Show both clients receiving identical increasing ids.
4. Stop one client while the server continues publishing.
5. Reconnect it and show missing records replayed before live records.
6. Stop and restart the server with the same WAL, then show id continuation.
7. Connect a client from id `0` and show WAL replay.
8. Explain the append-before-publish invariant, ring-to-WAL fallback, and slow-client bound.

## 13. Recommended Build Order

Implement Milestones 0 through 7 sequentially. Commit or checkpoint after every exit condition. Do not begin lock-free queues, CPU pinning, edge-triggered `epoll`, or zero-copy experiments until the correctness path is complete and profiling identifies a real bottleneck.

The core invariant throughout the implementation is:

> A news id becomes visible to a client only after its complete WAL record has been appended, and every client observes ids in strictly increasing order.
