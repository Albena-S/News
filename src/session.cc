#include "news/session.h"

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <utility>
#include <vector>

namespace news {

Session::Session(
    const int fd, const std::size_t receive_capacity,
    const std::size_t max_queued_bytes)
    : fd_(fd),
      receive_buffer_(receive_capacity),
      max_queued_bytes_(max_queued_bytes) {}

Session::~Session() {
  Close();
}

Session::Session(Session&& other)
    : fd_(std::exchange(other.fd_, -1)),
      state_(other.state_),
      receive_buffer_(std::move(other.receive_buffer_)),
      output_queue_(std::move(other.output_queue_)),
      output_offset_(other.output_offset_),
      queued_bytes_(other.queued_bytes_),
      max_queued_bytes_(other.max_queued_bytes_) {
  other.output_offset_ = 0;
  other.queued_bytes_ = 0;
}

Session& Session::operator=(Session&& other) {
  if (this == &other) {
    return *this;
  }

  Close();
  fd_ = std::exchange(other.fd_, -1);
  state_ = other.state_;
  receive_buffer_ = std::move(other.receive_buffer_);
  output_queue_ = std::move(other.output_queue_);
  output_offset_ = other.output_offset_;
  queued_bytes_ = other.queued_bytes_;
  max_queued_bytes_ = other.max_queued_bytes_;
  other.output_offset_ = 0;
  other.queued_bytes_ = 0;
  return *this;
}

IoResult Session::ReadAvailable() {
  constexpr std::size_t kReadChunkBytes = 4096;
  std::vector<std::byte> scratch(kReadChunkBytes);
  std::size_t total = 0;

  for (;;) {
    if (receive_buffer_.available() == 0) {
      const auto status =
          total > 0 ? IoStatus::kReady : IoStatus::kBufferFull;
      return {status, total, 0};
    }

    scratch.resize(kReadChunkBytes);
    const auto requested =
        std::min(scratch.size(), receive_buffer_.available());
    const auto received = ::recv(fd_, scratch.data(), requested, 0);
    if (received > 0) {
      const auto count = static_cast<std::size_t>(received);
      scratch.resize(count);
      if (!receive_buffer_.Append(scratch)) {
        return {IoStatus::kBufferFull, total, 0};
      }
      total += count;
      continue;
    }
    if (received == 0) {
      return {IoStatus::kPeerClosed, total, 0};
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {IoStatus::kReady, total, 0};
    }
    return {IoStatus::kError, total, errno};
  }
}

IoResult Session::FlushOutput() {
  std::size_t total = 0;

  while (!output_queue_.empty()) {
    const auto& frame = output_queue_.front();
    const auto* data = frame.data() + output_offset_;
    const auto remaining = frame.size() - output_offset_;
    const auto sent = ::send(fd_, data, remaining, MSG_NOSIGNAL);

    if (sent > 0) {
      const auto count = static_cast<std::size_t>(sent);
      total += count;
      queued_bytes_ -= count;
      output_offset_ += count;
      if (output_offset_ == frame.size()) {
        output_queue_.pop_front();
        output_offset_ = 0;
      }
      continue;
    }
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return {IoStatus::kReady, total, 0};
    }
    return {IoStatus::kError, total, sent < 0 ? errno : 0};
  }

  return {IoStatus::kReady, total, 0};
}

bool Session::QueueFrame(std::vector<std::byte> frame) {
  if (frame.size() > max_queued_bytes_ - queued_bytes_) {
    return false;
  }
  queued_bytes_ += frame.size();
  output_queue_.push_back(std::move(frame));
  return true;
}

int Session::fd() const {
  return fd_;
}

SessionState Session::state() const {
  return state_;
}

bool Session::has_pending_output() const {
  return !output_queue_.empty();
}

const std::vector<std::byte>& Session::received_data() const {
  return receive_buffer_.data();
}

std::size_t Session::received_offset() const {
  return receive_buffer_.read_offset();
}

std::size_t Session::received_size() const {
  return receive_buffer_.size();
}

void Session::ConsumeReceived(const std::size_t count) {
  receive_buffer_.Consume(count);
}

void Session::set_state(const SessionState state) {
  state_ = state;
}

void Session::Close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

}  // namespace news
