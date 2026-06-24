#ifndef NEWS_INCLUDE_NEWS_SESSION_H_
#define NEWS_INCLUDE_NEWS_SESSION_H_

#include <cstddef>
#include <deque>
#include <vector>

#include "news/byte_buffer.h"

namespace news {

enum class SessionState {
  kConnected,
  kAuthenticated,
  kLive,
  kClosing,
};

enum class IoStatus {
  kReady,
  kPeerClosed,
  kBufferFull,
  kError,
};

struct IoResult {
  IoStatus status{IoStatus::kReady};
  std::size_t bytes_transferred{0};
  int error_number{0};
};

/**
 * Stores the state and buffers for one connected client.
 *
 * A session owns the client socket, keeps received bytes until the server
 * decodes them, and queues encoded frames until the socket is ready to send.
 */
class Session {
 public:
  Session(int fd, std::size_t receive_capacity,
          std::size_t max_queued_bytes);
  ~Session();

  Session(Session&& other);
  Session& operator=(Session&& other);

  IoResult ReadAvailable();
  IoResult FlushOutput();
  bool QueueFrame(std::vector<std::byte> frame);

  int fd() const;
  SessionState state() const;
  bool has_pending_output() const;
  const std::vector<std::byte>& received_data() const;
  std::size_t received_offset() const;
  std::size_t received_size() const;

  void ConsumeReceived(std::size_t count);
  void set_state(SessionState state);

 private:
  void Close();

  int fd_{-1};
  SessionState state_{SessionState::kConnected};
  ByteBuffer receive_buffer_;
  std::deque<std::vector<std::byte>> output_queue_;
  std::size_t output_offset_{0};
  std::size_t queued_bytes_{0};
  std::size_t max_queued_bytes_{0};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_SESSION_H_
