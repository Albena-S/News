# Handover Summary for AI Agent

## 1. Project Context
*   **Goal**: Develop a C++ TCP news server with authentication, crash recovery, and stream resumption (reprise du flux).
*   **Target Domain**: High-performance / low-latency trading technical interview.
*   **User Environment**: WSL (Windows Subsystem for Linux) via VS Code.

## 2. Key Decisions & Constraints
*   **No Boost**: Explicitly disallowed by the interviewer.
*   **No Heavy Frameworks (e.g. POCO, Qt)**: Unsuitable for the low-latency context of a trading job.
*   **No Databases (e.g. SQLite)**: Too much overhead/blocking disk I/O on the critical path.
*   **Winsock/POSIX Raw Sockets**: Networking must be built using raw OS sockets (non-blocking with `epoll` or standard multiplexing). Since the user is on WSL, Linux `epoll` is fully available and highly relevant for high-performance networking.
*   **Binary Protocol**: Use custom length-prefixed binary frames rather than slow string protocols like JSON or CSV.
*   **Write-Ahead Log (WAL)**: Use a lightweight, append-only binary log file on disk to persist generated news articles. This handles crash recovery.
*   **In-Memory Ring Buffer**: Pre-allocated circular buffer containing the most recent $N$ messages for quick client stream resumption.
*   **FIX Replay Pattern**: Stream resumption based on monotonic sequence numbers (monotonically increasing IDs). On reconnect, the client passes their last seen ID, and the server replays subsequent records from the Ring Buffer or WAL.

## 3. Current Progress
*   [x] Analyzed the initial task requirements.
*   [x] Performed a state-of-the-art study on relevant open-source libraries (`tcpshm`, `quickfix`, `leveldb`, `SPSCQueue`, raw `epoll` servers).
*   [x] Documented research in `state_of_the_art_study.md`.
*   [ ] Draft the next version of `implementation_plan.md` focusing strictly on the low-latency Linux/WSL raw socket architecture.

## 4. Next Step for the Next Agent
*   Wait for the user to approve starting the implementation plan.
*   Update `implementation_plan.md` to detail a Linux-specific raw `epoll` socket server, zero-allocation memory pools, circular buffers, and an append-only binary log persistence layer.
*   Do NOT perform file modifications or start implementing until the user explicitly prompts to do so.
