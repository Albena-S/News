#include "news_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

#include "news/protocol.h"

namespace news {
namespace {

constexpr std::size_t kSequenceBytes = 8;

std::vector<std::byte> EncodeSequence(const std::uint64_t sequence) {
  std::vector<std::byte> encoded(kSequenceBytes);
  for (std::size_t index = 0; index < kSequenceBytes; ++index) {
    const auto shift = static_cast<unsigned int>(
        (kSequenceBytes - index - 1) * 8);
    encoded[index] = static_cast<std::byte>((sequence >> shift) & 0xffU);
  }
  return encoded;
}

}  // namespace

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

bool NewsClient::Subscribe(const std::uint64_t last_seen_sequence) {
  const auto payload = EncodeSequence(last_seen_sequence);
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
      std::cout << "news: ";
      for (const auto byte : frame.payload) {
        std::cout << static_cast<char>(std::to_integer<unsigned char>(byte));
      }
      std::cout << '\n';
    }

    received_data.resize(kMaxFrameBytes);
  }
}

}  // namespace news
