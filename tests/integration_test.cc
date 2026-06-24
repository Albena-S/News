#include "news/session.h"
#include "news_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace {

void TestSessionIo() {
  std::array<int, 2> sockets{-1, -1};
  const auto created =
      ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                   sockets.data());
  assert(created == 0);

  news::Session session(sockets[0], 16, 16);
  const std::array input{
    std::byte{'n'},
    std::byte{'e'},
    std::byte{'w'},
    std::byte{'s'},
  };

  const auto sent = ::send(sockets[1], input.data(), input.size(), 0);
  assert(sent == static_cast<ssize_t>(input.size()));

  const auto read_result = session.ReadAvailable();
  assert(read_result.status == news::IoStatus::kReady);
  assert(read_result.bytes_transferred == input.size());
  assert(session.received_offset() == 0);
  assert(std::ranges::equal(session.received_data(), input));
  session.ConsumeReceived(input.size());
  assert(session.received_size() == 0);

  assert(session.QueueFrame(std::vector(input.begin(), input.end())));
  const auto write_result = session.FlushOutput();
  assert(write_result.status == news::IoStatus::kReady);
  assert(write_result.bytes_transferred == input.size());
  assert(!session.has_pending_output());

  std::array<std::byte, 4> output{};
  const auto received = ::recv(sockets[1], output.data(), output.size(), 0);
  assert(received == static_cast<ssize_t>(output.size()));
  assert(std::ranges::equal(output, input));

  ::close(sockets[1]);
}

void TestClientConnect() {
  const int listener_fd =
      ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  assert(listener_fd >= 0);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = 0;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  const auto bound = ::bind(
      listener_fd, reinterpret_cast<const sockaddr*>(&address),
      sizeof(address));
  assert(bound == 0);
  assert(::listen(listener_fd, 1) == 0);

  socklen_t address_size = sizeof(address);
  assert(::getsockname(listener_fd, reinterpret_cast<sockaddr*>(&address),
                       &address_size) == 0);

  news::NewsClient client("127.0.0.1", ntohs(address.sin_port));
  assert(client.Connect());

  const int client_fd = ::accept(listener_fd, nullptr, nullptr);
  assert(client_fd >= 0);

  ::close(client_fd);
  ::close(listener_fd);
}

}  // namespace

int main() {
  TestSessionIo();
  TestClientConnect();
  return 0;
}
