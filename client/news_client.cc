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

constexpr std::chrono::seconds kReconnectDelay{1};

}  // namespace

NewsClient::NewsClient(std::string address, const std::uint16_t port)
    : address_(std::move(address)), port_(port) {}

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

    received_data.resize(static_cast<std::size_t>(received));
    const auto frame = DecodeFrame(received_data);

    if (frame.type == MessageType::kNews) {
      const auto record = DecodeNews(frame.payload);
      last_received_id_ = record.id;
      std::cout << "news " << record.id << ": " << record.title << '\n';
    }

    received_data.resize(kMaxFrameBytes);
  }
}

void NewsClient::Run(
    const std::string& username, const std::string& password) {
  for (;;) {
    if (Connect() && Authenticate(username, password) &&
        Subscribe(last_received_id_)) {
      ReceiveNews();
    }

    Close();
    std::cout << "disconnected; reconnecting from news id "
              << last_received_id_ << '\n';
    std::this_thread::sleep_for(kReconnectDelay);
  }
}

void NewsClient::Close() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

}  // namespace news
