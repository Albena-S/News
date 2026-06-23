#include "news_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "news/protocol.h"

namespace news {

NewsClient::NewsClient(std::string address, const std::uint16_t port)
    : address_(std::move(address)), port_(port) {}

NewsClient::~NewsClient() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
  }
}

bool NewsClient::Connect() {
  socket_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (socket_fd_ < 0) {
    std::cerr << "could not create client socket\n";
    return false;
  }

  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_);
  ::inet_pton(AF_INET, address_.c_str(), &server_address.sin_addr);

  if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_address),
                sizeof(server_address)) < 0) {
    std::cerr << "could not connect to server\n";
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
    std::cerr << "could not send authentication\n";
    return false;
  }

  std::vector<std::byte> received_data(kMaxFrameBytes);
  const auto received =
      ::recv(socket_fd_, received_data.data(), received_data.size(), 0);
  if (received <= 0) {
    std::cerr << "could not receive authentication result\n";
    return false;
  }

  received_data.resize(static_cast<std::size_t>(received));
  const auto response = DecodeFrame(received_data);
  if (response.type != MessageType::kAuthResult) {
    std::cerr << "unexpected authentication response\n";
    return false;
  }

  if (!DecodeAuthResult(response.payload)) {
    std::cerr << "authentication refused\n";
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
    std::cerr << "could not send subscription\n";
    return false;
  }

  std::cout << "subscribed; waiting for news\n";
  return true;
}

void NewsClient::ReceiveNews() {
  std::vector<std::byte> received_data(kMaxFrameBytes);

  while (true) {
    const auto received =
        ::recv(socket_fd_, received_data.data(), received_data.size(), 0);
    if (received <= 0) {
      return;
    }

    received_data.resize(static_cast<std::size_t>(received));
    const auto frame = DecodeFrame(received_data);

    if (frame.type == MessageType::kNews) {
      const auto record = DecodeNews(frame.payload);
      std::cout << "news " << record.id << ": " << record.title << '\n';
    }

    received_data.resize(kMaxFrameBytes);
  }
}

}  // namespace news
