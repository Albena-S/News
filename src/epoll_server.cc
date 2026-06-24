#include "news/epoll_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "news/protocol.h"

namespace news {
namespace {

void ThrowSystemError(const char* operation) {
  throw std::system_error(errno, std::generic_category(), operation);
}

void CloseFd(int& fd) {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

}  // namespace

EpollServer::EpollServer(ServerConfig config)
    : config_(std::move(config)),
      authenticator_(config_.users_file_path),
      wal_(config_.wal_file_path),
      replay_ring_(config_.replay_ring_capacity) {
  try {
    const auto recovered_records = wal_.Recover();
    for (const auto& record : recovered_records) {
      replay_ring_.Append(record);
      next_id_ = record.id + 1;
    }

    CreateListener();
    CreateEpoll();
    CreateSignalFd();
    AddToEpoll(listener_fd_, EPOLLIN);
    AddToEpoll(signal_fd_, EPOLLIN);
    AddToEpoll(publisher_.event_fd(), EPOLLIN);
    publisher_.Start();
  } catch (...) {
    CloseAll();
    throw;
  }
}

EpollServer::~EpollServer() {
  CloseAll();
}

void EpollServer::Run() {
  constexpr int max_events = 64;
  std::array<epoll_event, max_events> events{};
  running_ = true;

  std::cout << "news server listening on " << config_.bind_address << ':'
            << bound_port_ << '\n';

  try {
    while (running_) {
      const auto ready = ::epoll_wait(epoll_fd_, events.data(), max_events, -1);
      if (ready < 0) {
        if (errno == EINTR) {
          continue;
        }
        ThrowSystemError("epoll_wait");
      }

      for (int index = 0; index < ready && running_; ++index) {
        const auto fd = events[static_cast<std::size_t>(index)].data.fd;
        const auto event_flags = events[static_cast<std::size_t>(index)].events;

        if (fd == listener_fd_) {
          AcceptConnections();
        } else if (fd == signal_fd_) {
          HandleSignal();
        } else if (fd == publisher_.event_fd()) {
          HandlePublisherEvent();
        } else {
          HandleSessionEvent(fd, event_flags);
        }
      }
    }
  } catch (...) {
    running_ = false;
    throw;
  }

  std::cout << "news server stopped\n";
}

std::uint16_t EpollServer::port() const {
  return bound_port_;
}

void EpollServer::CreateListener() {
  listener_fd_ =
      ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listener_fd_ < 0) {
    ThrowSystemError("socket");
  }

  const int enabled = 1;
  if (::setsockopt(listener_fd_, SOL_SOCKET, SO_REUSEADDR, &enabled,
                   sizeof(enabled)) < 0) {
    ThrowSystemError("setsockopt(SO_REUSEADDR)");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.port);
  if (::inet_pton(AF_INET, config_.bind_address.c_str(), &address.sin_addr) !=
      1) {
    throw std::invalid_argument("bind address must be a valid IPv4 address");
  }

  if (::bind(listener_fd_, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) < 0) {
    ThrowSystemError("bind");
  }
  if (::listen(listener_fd_, config_.listen_backlog) < 0) {
    ThrowSystemError("listen");
  }

  socklen_t address_size = sizeof(address);
  if (::getsockname(listener_fd_, reinterpret_cast<sockaddr*>(&address),
                    &address_size) < 0) {
    ThrowSystemError("getsockname");
  }
  bound_port_ = ntohs(address.sin_port);
}

void EpollServer::CreateEpoll() {
  epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    ThrowSystemError("epoll_create1");
  }
}

void EpollServer::CreateSignalFd() {
  sigset_t mask{};
  ::sigemptyset(&mask);
  ::sigaddset(&mask, SIGINT);
  ::sigaddset(&mask, SIGTERM);

  if (::sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
    ThrowSystemError("sigprocmask");
  }

  signal_fd_ = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (signal_fd_ < 0) {
    ThrowSystemError("signalfd");
  }
}

void EpollServer::AddToEpoll(const int fd, const std::uint32_t events) {
  epoll_event event{};
  event.events = events;
  event.data.fd = fd;
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    ThrowSystemError("epoll_ctl(ADD)");
  }
}

void EpollServer::UpdateSessionEvents(const Session& session) {
  epoll_event event{};
  event.events = EPOLLIN | EPOLLRDHUP;
  if (session.has_pending_output()) {
    event.events |= EPOLLOUT;
  }
  event.data.fd = session.fd();

  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, session.fd(), &event) < 0) {
    ThrowSystemError("epoll_ctl(MOD)");
  }
}

void EpollServer::AcceptConnections() {
  for (;;) {
    sockaddr_in peer{};
    socklen_t peer_size = sizeof(peer);
    const auto client_fd = ::accept4(
      listener_fd_,
      reinterpret_cast<sockaddr*>(&peer),
      &peer_size,
      SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      ThrowSystemError("accept4");
    }

    if (config_.tcp_no_delay) {
      const int enabled = 1;
      if (::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enabled,
                       sizeof(enabled)) < 0) {
        std::cerr << "closing client: failed to set TCP_NODELAY: "
                  << std::strerror(errno) << '\n';
        ::close(client_fd);
        continue;
      }
    }

    Session session(client_fd, config_.receive_buffer_bytes,
                    config_.max_queued_bytes);
    sessions_.emplace(client_fd, std::move(session));

    try {
      AddToEpoll(client_fd, EPOLLIN | EPOLLRDHUP);
    } catch (const std::system_error& error) {
      std::cerr << "closing client: " << error.what() << '\n';
      sessions_.erase(client_fd);
      continue;
    }

    std::array<char, INET_ADDRSTRLEN> peer_address{};
    const char* formatted =
        ::inet_ntop(AF_INET, &peer.sin_addr, peer_address.data(),
                    peer_address.size());
    std::cout << "client connected: "
              << (formatted != nullptr ? formatted : "unknown") << ':'
              << ntohs(peer.sin_port) << " fd=" << client_fd << '\n';
  }
}

void EpollServer::HandleSessionEvent(const int fd,
                                     const std::uint32_t events) {
  const auto found = sessions_.find(fd);
  if (found == sessions_.end()) {
    return;
  }
  auto& session = found->second;

  if ((events & EPOLLIN) != 0U) {
    const auto result = session.ReadAvailable();

    HandleReceivedFrame(session);

    if (result.status == IoStatus::kPeerClosed) {
      CloseSession(fd, "peer closed connection");
      return;
    }
    if (result.status == IoStatus::kBufferFull) {
      CloseSession(fd, "receive buffer limit reached");
      return;
    }
    if (result.status == IoStatus::kError) {
      CloseSession(fd, std::strerror(result.error_number));
      return;
    }
  }

  if ((events & EPOLLOUT) != 0U) {
    const auto result = session.FlushOutput();
    if (result.status == IoStatus::kError) {
      CloseSession(fd, std::strerror(result.error_number));
      return;
    }
  }

  if (session.state() == SessionState::kClosing &&
      !session.has_pending_output()) {
    CloseSession(fd, "authentication refused");
    return;
  }

  if ((events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) {
    CloseSession(fd, "socket hangup");
    return;
  }

  UpdateSessionEvents(session);
}

void EpollServer::HandleReceivedFrame(Session& session) {
  if (session.received_size() == 0) {
    return;
  }

  const auto begin = session.received_data().begin() +
                     static_cast<std::ptrdiff_t>(session.received_offset());
  const auto end = begin + static_cast<std::ptrdiff_t>(session.received_size());
  const std::vector<std::byte> received_frame(begin, end);
  const auto frame = DecodeFrame(received_frame);

  if (frame.type == MessageType::kAuthRequest) {
    const auto credentials = DecodeAuthRequest(frame.payload);
    const auto accepted = authenticator_.Authenticate(
        credentials.username, credentials.password);
    session.QueueFrame(
        EncodeFrame(MessageType::kAuthResult, EncodeAuthResult(accepted)));

    if (accepted) {
      session.set_state(SessionState::kAuthenticated);
    } else {
      session.set_state(SessionState::kClosing);
    }
  } else if (frame.type == MessageType::kSubscribe &&
             session.state() == SessionState::kAuthenticated) {
    const auto last_seen_id = DecodeSubscribe(frame.payload);
    const auto newest_id = next_id_ - 1;
    const auto first_replay_id =
        last_seen_id > newest_id ? next_id_ : last_seen_id + 1;
    const auto replay_records = ReplayRecordsFrom(first_replay_id);
    for (const auto& record : replay_records) {
      session.QueueFrame(EncodeFrame(MessageType::kNews, EncodeNews(record)));
    }
    session.set_state(SessionState::kLive);
  }

  session.ConsumeReceived(session.received_size());
}

void EpollServer::HandlePublisherEvent() {
  std::uint64_t wake_count = 0;
  for (;;) {
    const auto bytes =
        ::read(publisher_.event_fd(), &wake_count, sizeof(wake_count));
    if (bytes == static_cast<ssize_t>(sizeof(wake_count))) {
      continue;
    }
    if (bytes < 0 && errno == EINTR) {
      continue;
    }
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    if (bytes < 0) {
      ThrowSystemError("read(publisher eventfd)");
    }
    break;
  }

  const auto titles = publisher_.TakeTitles();
  for (const auto& title : titles) {
    PublishTitle(title);
  }
}

std::vector<NewsRecord> EpollServer::ReplayRecordsFrom(
    const std::uint64_t first_id) const {
  const auto oldest_id = replay_ring_.oldest_id();
  if (oldest_id != 0 && first_id >= oldest_id) {
    return replay_ring_.From(first_id);
  }

  return wal_.From(first_id);
}

void EpollServer::PublishTitle(const std::string& title) {
  NewsRecord record;
  record.id = next_id_;
  record.title = title;

  if (!wal_.Append(record)) {
    throw std::runtime_error("could not append news to WAL");
  }

  ++next_id_;
  replay_ring_.Append(record);
  BroadcastNews(record);

  std::cout << "published news " << record.id << ": " << record.title << '\n';
}

void EpollServer::BroadcastNews(const NewsRecord& record) {
  const auto frame = EncodeFrame(MessageType::kNews, EncodeNews(record));
  for (auto& [fd, session] : sessions_) {
    if (session.state() == SessionState::kLive) {
      session.QueueFrame(frame);
      UpdateSessionEvents(session);
    }
  }
}

void EpollServer::HandleSignal() {
  signalfd_siginfo signal_info{};
  for (;;) {
    const auto bytes = ::read(signal_fd_, &signal_info, sizeof(signal_info));
    if (bytes == static_cast<ssize_t>(sizeof(signal_info))) {
      if (signal_info.ssi_signo == SIGINT ||
          signal_info.ssi_signo == SIGTERM) {
        running_ = false;
      }
      continue;
    }
    if (bytes < 0 && errno == EINTR) {
      continue;
    }
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }
    if (bytes < 0) {
      ThrowSystemError("read(signalfd)");
    }
    return;
  }
}

void EpollServer::CloseSession(const int fd, const char* reason) {
  std::cout << "client disconnected: fd=" << fd << " reason=" << reason
            << '\n';
  sessions_.erase(fd);
}

void EpollServer::CloseAll() {
  publisher_.Stop();
  sessions_.clear();
  CloseFd(signal_fd_);
  CloseFd(listener_fd_);
  CloseFd(epoll_fd_);
}

}  // namespace news
