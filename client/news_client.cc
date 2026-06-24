#include "news_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "news/protocol.h"

namespace news {
namespace {

constexpr std::chrono::seconds kReconnectDelay{5};

}  // namespace

NewsClient::NewsClient(
    std::string address, const std::uint16_t port,
    const std::uint64_t last_received_id)
    : address_(std::move(address)),
      port_(port),
      last_received_id_(last_received_id) {}

NewsClient::~NewsClient() {
  Close();
}

bool NewsClient::Connect() {
  Close();

  socket_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (socket_fd_ < 0) {
    std::cerr << "could not create client socket: " << std::strerror(errno)
              << '\n';
    return false;
  }

  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_);
  if (::inet_pton(AF_INET, address_.c_str(), &server_address.sin_addr) != 1) {
    std::cerr << "invalid server address: " << address_ << '\n';
    Close();
    return false;
  }

  if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_address),
                sizeof(server_address)) < 0) {
    std::cerr << "could not connect to " << address_ << ':' << port_ << ": "
              << std::strerror(errno) << '\n';
    Close();
    return false;
  }

  return true;
}

bool NewsClient::Authenticate(
    const std::string& username, const std::string& password) {
  const auto payload = EncodeAuthRequest(username, password);
  const auto frame = EncodeFrame(MessageType::kAuthRequest, payload);
  const auto sent = ::send(socket_fd_, frame.data(), frame.size(), 0);
  if (sent < 0 || static_cast<std::size_t>(sent) != frame.size()) {
    if (sent < 0) {
      std::cerr << "could not send authentication request: "
                << std::strerror(errno) << '\n';
    } else {
      std::cerr << "could not send complete authentication request\n";
    }
    return false;
  }

  std::vector<std::byte> received_data(kMaxFrameBytes);
  const auto received =
      ::recv(socket_fd_, received_data.data(), received_data.size(), 0);
  if (received <= 0) {
    if (received < 0) {
      std::cerr << "could not receive authentication result: "
                << std::strerror(errno) << '\n';
    } else {
      std::cerr << "server closed connection before authentication result\n";
    }
    return false;
  }

  received_data.resize(static_cast<std::size_t>(received));
  const auto response = DecodeFrame(received_data);
  if (response.type != MessageType::kAuthResult) {
    std::cerr << "unexpected authentication response\n";
    return false;
  }

  if (!DecodeAuthResult(response.payload)) {
    std::cerr << "authentication refused: username or password did not match\n";
    return false;
  }

  std::cout << "authenticated\n";
  return true;
}

bool NewsClient::Subscribe(const std::uint64_t last_seen_id) {
  const auto payload = EncodeSubscribe(last_seen_id);
  const auto frame = EncodeFrame(MessageType::kSubscribe, payload);
  const auto sent = ::send(socket_fd_, frame.data(), frame.size(), 0);
  if (sent < 0 || static_cast<std::size_t>(sent) != frame.size()) {
    if (sent < 0) {
      std::cerr << "could not send subscription: " << std::strerror(errno)
                << '\n';
    } else {
      std::cerr << "could not send complete subscription\n";
    }
    return false;
  }

  std::cout << "subscribed from news id " << last_seen_id
            << "; waiting for news\n";
  return true;
}

void NewsClient::ReceiveNews() {
  std::vector<std::byte> received_data(kMaxFrameBytes);
  std::vector<std::byte> pending_data;

  while (true) {
    const auto received =
        ::recv(socket_fd_, received_data.data(), received_data.size(), 0);
    if (received <= 0) {
      if (received < 0) {
        std::cerr << "could not receive news: " << std::strerror(errno)
                  << '\n';
      } else {
        std::cerr << "server closed connection\n";
      }
      return;
    }

    const auto received_size = static_cast<std::size_t>(received);
    pending_data.insert(pending_data.end(), received_data.begin(),
                        received_data.begin() +
                            static_cast<std::ptrdiff_t>(received_size));

    std::size_t offset = 0;
    while (pending_data.size() - offset >= kFrameHeaderBytes) {
      const auto frame_size = EncodedFrameSize(pending_data, offset);
      if (pending_data.size() - offset < frame_size) {
        break;
      }

      const auto frame = DecodeFrameAt(pending_data, offset);
      if (frame.type == MessageType::kNews) {
        const auto record = DecodeNews(frame.payload);
        last_received_id_ = record.id;
        std::cout << "news " << record.id << ": " << record.title << '\n';
      }

      offset += frame_size;
    }

    if (offset > 0) {
      pending_data.erase(pending_data.begin(),
                         pending_data.begin() +
                             static_cast<std::ptrdiff_t>(offset));
    }
  }
}

void NewsClient::Run(
    const std::string& username, const std::string& password) {
  if (!Connect() || !Authenticate(username, password) ||
      !Subscribe(last_received_id_)) {
    Close();
    return;
  }

  for (;;) {
    ReceiveNews();

    Close();
    std::cout << "disconnected; reconnecting from news id "
              << last_received_id_ << '\n';
    while (!Connect() || !Authenticate(username, password) ||
           !Subscribe(last_received_id_)) {
      Close();
      std::cout << "reconnect failed; retrying in 5 seconds\n";
      std::this_thread::sleep_for(kReconnectDelay);
    }
  }
}

void NewsClient::Close() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

}  // namespace news
