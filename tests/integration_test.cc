#include "news/session.h"

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

int main() {
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
  return 0;
}
