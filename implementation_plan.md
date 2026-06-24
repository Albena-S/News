# Implementation Plan: Low-Latency C++ TCP News Server

## 1. Objective

Build an interview-ready C++ news streaming system that runs on Linux/WSL and demonstrates:

- authenticated TCP client sessions;
- non-blocking networking with raw POSIX sockets and `epoll`;
- a length-prefixed binary protocol;
- monotonically increasing news sequence numbers;
- crash recovery through an append-only write-ahead log (WAL);
- stream resumption from the last sequence received by a client;
- fast replay of recent messages from a preallocated in-memory ring buffer;
- bounded memory use and explicit slow-client handling.

The implementation will not use Boost, a networking framework, or a database.

## 2. Scope and Success Criteria

The first complete version is successful when it can:

1. Start, recover all valid WAL records, and continue with the next sequence number.
2. Accept multiple non-blocking TCP clients through one `epoll` event loop.
3. Authenticate a client before allowing subscription or replay.
4. Publish each news item only after it has been appended to the WAL.
5. Deliver live news in sequence-number order.
6. Reconnect a client using its last received sequence number and replay every later item.
7. Serve recent replay from memory and older replay from the WAL.
8. Survive a deliberately truncated or partially written final WAL record.
9. Disconnect or throttle a client whose output queue exceeds a configured bound.
10. Build and pass automated tests under WSL using CMake and CTest.

TLS, distributed replication, user administration, and production-grade secret storage are explicitly outside the initial scope.

## 3. Proposed Architecture

### 3.1 Runtime components

- **Main/event-loop thread**: owns the listening socket, all client sockets, session state, protocol parsing, replay scheduling, and live broadcast.
- **News source**: initially a deterministic timer-driven generator. A Linux `timerfd` integrates it directly into `epoll` without another thread.
- **WAL**: one append-only binary file. The event loop appends a complete record before making that news item visible to clients.
- **Replay ring**: a fixed-capacity circular array holding the latest decoded news records.
- **Signal handling**: a `signalfd` registered with `epoll` enables orderly shutdown without asynchronous signal-handler logic.

This single-owner design avoids locks in the socket and session hot path. If synchronous WAL latency later proves unacceptable, Phase 8 moves persistence to a dedicated writer thread connected with bounded SPSC queues and an `eventfd`; the event loop still publishes only after receiving a durability acknowledgement.

### 3.2 Data flow

```text
timerfd/news source
        |
        v
assign sequence -> encode WAL record -> append/durability policy
        |                                      |
        | append succeeds                      | append fails
        v                                      v
insert into replay ring -> broadcast       stop publishing,
to authenticated live clients              report fatal error

reconnecting client -> authenticate -> provide last_seen_seq
                                      |
                     +----------------+----------------+
                     |                                 |
              sequence in ring                  sequence too old
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
|   |-- protocol.cc
|   |-- session.cc
|   |-- epoll_server.cc
|   |-- wal.cc
|   |-- replay_ring.cc
|   `-- server_main.cc
|-- client/
|   `-- client_main.cc
|-- tests/
|   |-- protocol_test.cc
|   |-- wal_test.cc
|   |-- replay_ring_test.cc
|   `-- integration_test.cc
`-- scripts/
    `-- demo_recovery.sh
```

The sample client is part of the deliverable: it proves authentication, sequence tracking, reconnection, and replay without relying on external tools.

## 5. Binary Protocol

### 5.1 Transport frame

TCP is a byte stream, so every application message uses an explicit frame:

```text
FrameHeader (network byte order)
  uint32 payload_length
  uint16 protocol_version
  uint16 message_type
Payload[payload_length]
```

Rules:

- The header has a fixed encoded size; do not transmit C++ structs directly.
- Integers are explicitly encoded/decoded in big-endian order.
- `payload_length` excludes the frame header and is capped, initially at 64 KiB.
- Unknown versions, unknown message types, malformed lengths, and invalid state transitions produce an error frame followed by disconnect.
- The parser must support fragmented headers, fragmented payloads, and multiple frames in one `recv()` call.

### 5.2 Initial message types

| Type | Direction | Purpose |
|---|---|---|
| `AUTH_REQUEST` | Client to server | Username and password for the exercise |
| `AUTH_RESULT` | Server to client | Authentication success or failure |
| `SUBSCRIBE` | Client to server | Contains `last_seen_seq` |
| `NEWS` | Server to client | Sequence, timestamp, topic, and body |
| `HEARTBEAT` | Both | Detect idle or dead peers |
| `ERROR` | Server to client | Protocol/session error code |

`NEWS` payload:

```text
uint64 sequence
uint64 timestamp_ns
uint16 topic_length
uint32 body_length
byte[] topic
byte[] body
```

Lengths and maximum field sizes are validated before allocation or copying.

### 5.3 Session state machine

```text
CONNECTED -> AUTHENTICATED -> REPLAYING -> LIVE -> CLOSING
     |             |              |
     +-------------+--------------+--> CLOSING on error/timeout
```

- Only `AUTH_REQUEST` is legal in `CONNECTED`.
- Only `SUBSCRIBE` is legal in `AUTHENTICATED`.
- Live news generated while a client is replaying is not interleaved incorrectly. The session captures a replay high-water mark, replays through it, then queues later live records in sequence order before entering `LIVE`.
- Authentication uses a fixed configuration file for the exercise. Plaintext credentials are acceptable only on the local demo network; the README must state that production deployment requires TLS or challenge-response authentication and hashed secrets.

## 6. Persistence and Crash Recovery

### 6.1 WAL file format

Start the file with a magic value and format version. Each record is self-validating:

```text
FileHeader:
  magic[8], format_version, reserved

Record:
  uint32 record_length
  uint32 crc32
  uint64 sequence
  uint64 timestamp_ns
  uint16 topic_length
  uint32 body_length
  topic bytes
  body bytes
```

The CRC covers the encoded record body, not `record_length` or the CRC field. WAL values use one documented byte order and are serialized field by field.

### 6.2 Append policy

- Open with `O_APPEND | O_CREAT | O_WRONLY | O_CLOEXEC`.
- Fully encode a bounded record before calling `write()`.
- Handle `EINTR` and partial writes.
- Never broadcast a record if its append fails.
- Provide two explicit durability modes:
  - `append`: publish after the kernel accepts all bytes;
  - `fdatasync`: call `fdatasync()` before publishing.
- Default the correctness demo to `fdatasync`; benchmarks must state which mode they use.

An append alone protects ordering and process-crash recovery but does not guarantee survival of sudden power loss. This distinction must be documented rather than hidden behind the word “durable.”

### 6.3 Startup recovery

On startup:

1. Validate the file header.
2. Read records sequentially.
3. Reject impossible lengths before reading payload data.
4. Validate CRC and strict sequence continuity.
5. Add valid records to the replay ring, naturally retaining only the newest capacity.
6. Record WAL offsets in a lightweight sequence-to-offset index for older replay.
7. If the final record is incomplete or has a bad CRC, treat it as a torn tail and truncate to the last valid offset.
8. Treat corruption in the middle of the file as fatal; do not silently skip it.
9. Set `next_sequence = last_valid_sequence + 1`.

The in-memory WAL index may begin as one offset per record. If scale becomes relevant, use sparse checkpoints and scan forward from the nearest checkpoint.

## 7. Replay Ring and Resumption

Use a preallocated circular array with power-of-two capacity. Because the event loop is the sole owner in the initial architecture, it does not need atomics or a concurrent queue.

The ring exposes:

- `append(NewsRecord)`;
- `oldest_sequence()` and `newest_sequence()`;
- lookup/iteration over an inclusive sequence range;
- an explicit result for “requested sequence has been evicted.”

On `SUBSCRIBE(last_seen_seq)`:

1. Reject `last_seen_seq` greater than the server's latest sequence.
2. Compute `first_required = last_seen_seq + 1`, guarding overflow.
3. Capture the current latest sequence as the replay high-water mark.
4. If `first_required` is in the ring, enqueue the range from memory.
5. Otherwise, read the older prefix from the WAL, then use the ring for the remaining range.
6. Preserve exact sequence order and avoid duplicates at the WAL/ring boundary.
7. Finish queued catch-up records before transitioning the session to live delivery.

Replay is incremental: each event-loop turn enqueues only up to a configured byte/record budget so that one reconnecting client cannot starve other clients.

## 8. Networking and Backpressure

### 8.1 Socket setup

- Create non-blocking, close-on-exec sockets (`SOCK_NONBLOCK | SOCK_CLOEXEC`).
- Set `SO_REUSEADDR`; make `TCP_NODELAY` configurable and enabled for latency tests.
- Use `accept4()` until it returns `EAGAIN`.
- Register the listener, clients, `timerfd`, and `signalfd` with `epoll`.
- Start with level-triggered `epoll` for simpler correctness. Consider edge-triggered mode only after tests and measurements justify it.
- Ignore `SIGPIPE` or send with `MSG_NOSIGNAL`.

### 8.2 Per-session buffers

Each session owns:

- a bounded receive buffer and incremental frame-parser state;
- authentication and subscription state;
- a FIFO transmit queue of immutable encoded frames or buffer slices;
- an offset into the front frame for partial `send()` completion;
- timestamps for authentication, idle, and heartbeat timeouts.

Enable `EPOLLOUT` only when a session has pending bytes, and disable it when the queue drains.

### 8.3 Slow-client policy

Memory must remain bounded. Configure a maximum queued byte count per session. If adding another frame would exceed it:

- send an error if space permits;
- otherwise close immediately;
- log the reason and last delivered sequence.

The initial policy is disconnect rather than dropping news, because silently creating a sequence gap violates the protocol. The client can reconnect and resume later.

## 9. Implementation Milestones

### Milestone 0: Build skeleton

- Add CMake targets for a core library, server, client, and tests.
- Enable C++20, warnings, and debug sanitizers through build options.
- Add a small configuration object for port, WAL path, ring capacity, limits, and durability mode.
- Add a README with WSL build/run commands.

**Exit condition:** clean build and one smoke test through CTest.

### Milestone 1: Protocol codec and parser

- Implement endian-safe integer helpers.
- Implement frame encoding without transmitting native structs.
- Implement the incremental receive parser and all size checks.
- Add tests for split input, coalesced frames, malformed sizes, unknown types, and round trips.

**Exit condition:** parser tests pass under AddressSanitizer and UndefinedBehaviorSanitizer.

### Milestone 2: WAL and recovery

- Implement file/record encoding, CRC32, append, durability modes, and sequential recovery.
- Build the sequence-to-offset index.
- Add tests for empty WAL, normal recovery, restart sequence continuity, truncated header/body, bad tail CRC, middle corruption, and append failure.

**Exit condition:** repeated crash/truncation simulations recover exactly the valid prefix.

### Milestone 3: Replay ring

- Implement fixed-capacity storage and overwrite behavior.
- Add boundary tests for empty, partially full, full, wrapped, evicted, and invalid ranges.
- Integrate ring reconstruction into WAL recovery.

**Exit condition:** every requested retained range is returned once and in order.

### Milestone 4: Non-blocking server and authentication

- Implement listener creation, the `epoll` loop, accept/read/write draining, session cleanup, and signal-driven shutdown.
- Implement the session state machine and fixed-file credential loading.
- Add timeouts for clients that connect but never authenticate.

**Exit condition:** multiple clients can authenticate concurrently; fragmented requests and partial writes work correctly.

### Milestone 5: Publishing and live delivery

- Add `timerfd`-driven deterministic news generation.
- Assign sequence numbers, append to WAL, update the ring, and broadcast in that exact order.
- Complete the sample client so it persists or displays its last received sequence.

**Exit condition:** all connected clients observe the same gap-free sequence stream.

### Milestone 6: Replay and reconnect

- Implement memory replay, WAL replay, high-water-mark handling, and incremental replay budgets.
- Add reconnect support to the client.
- Test disconnect/reconnect inside ring coverage and beyond ring coverage.

**Exit condition:** a client reconnects after missing messages and receives each missing sequence exactly once before live delivery.

### Milestone 7: Fault handling and integration tests

- Enforce all receive, transmit, and replay limits.
- Test slow readers, abrupt disconnects, invalid credentials, malformed frames, stale/future resume values, WAL write failure, server kill/restart, and clean shutdown.
- Add a reproducible recovery demo script.

**Exit condition:** integration tests prove bounded memory, ordered recovery, and continued service to healthy clients.

### Milestone 8: Measurement-driven optimization

- Add latency histograms for append-to-send and replay throughput.
- Benchmark `append` versus `fdatasync` durability.
- Preallocate/reuse buffers and encoded live frames where profiling shows allocation pressure.
- Consider `writev()`/`sendmsg()` batching.
- If WAL calls dominate event-loop latency, add a dedicated persistence thread, bounded SPSC command/acknowledgement queues, and `eventfd` notification.
- Consider CPU affinity and edge-triggered `epoll` only after correctness remains covered by tests.

**Exit condition:** benchmark results and architecture trade-offs are documented; optimizations are supported by measurements.

## 10. Test Strategy

### Unit tests

- Protocol encode/decode and incremental parsing.
- WAL serialization, CRC, append, scan, and truncation recovery.
- Ring wraparound and range retrieval.
- Session state transitions and illegal-message handling.

### Integration tests

- Start the server on an ephemeral port and connect real TCP clients.
- Authenticate, subscribe from zero, and validate ordered news.
- Disconnect, allow publication to continue, reconnect, and validate replay.
- Restart the server with the same WAL and validate sequence continuation.
- Force a torn WAL tail and validate prefix recovery.
- Keep one client from reading while another remains healthy.
- Send frames one byte at a time and several frames in one write.

### Tooling

- Debug builds: `-Wall -Wextra -Wpedantic -Wconversion` plus ASan/UBSan.
- Race checking after introducing any worker thread: ThreadSanitizer in a separate build.
- System-call inspection: `strace`.
- CPU and allocation profiling: `perf` and an appropriate heap profiler if needed.
- Network robustness: loopback tests plus optional `tc netem` latency/loss testing.

## 11. Operational and Security Notes

- Validate configuration before opening the listening socket.
- Apply restrictive permissions to the WAL and credential file.
- Never log passwords or raw authentication payloads.
- Log connection ID, state transition, replay range, disconnect reason, and last delivered sequence.
- Use structured, rate-limited logs outside the send/receive hot path where practical.
- Exit on unrecoverable WAL corruption or append failure rather than serving data that is not recoverable.
- Document maximum frame size, ring capacity, output-queue limit, timeout values, and durability mode.

## 12. Interview Demonstration

The final demonstration should be short and observable:

1. Build and run all tests.
2. Start the server and two authenticated clients.
3. Show both clients receiving identical increasing sequences.
4. Stop one client while the server continues publishing.
5. Reconnect it and show missing records replayed before live records.
6. Kill the server ungracefully, restart it with the same WAL, and show sequence continuation.
7. Explain the append-before-publish invariant, torn-tail recovery, ring-to-WAL fallback, and slow-client bound.
8. Present benchmark numbers with the durability mode stated explicitly.

## 13. Recommended Build Order

Implement Milestones 0 through 7 sequentially. Commit or checkpoint after every exit condition. Do not begin lock-free queues, CPU pinning, edge-triggered `epoll`, or zero-copy experiments until the correctness path is complete and profiling identifies a real bottleneck.

The core invariant throughout the implementation is:

> A sequence becomes visible to a client only after its complete WAL record has been accepted under the configured durability policy, and every client observes sequences in strictly increasing order.
