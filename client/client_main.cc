#include "news_client.h"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kDefaultAddress = "127.0.0.1";
constexpr std::uint16_t kDefaultPort = 9000;
constexpr std::string_view kDefaultUsername = "demo";
constexpr std::string_view kDefaultPassword = "demo";
constexpr std::uint64_t kDefaultLastReceivedId =
    std::numeric_limits<std::uint64_t>::max();

std::uint16_t ParsePort(const std::string_view text) {
  unsigned int port = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), port);
  if (error != std::errc{} || end != text.data() + text.size() ||
      port > 65'535U) {
    throw std::invalid_argument("port must be an integer from 0 to 65535");
  }
  return static_cast<std::uint16_t>(port);
}

std::uint64_t ParseId(const std::string_view text) {
  std::uint64_t id = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), id);
  if (error != std::errc{} || end != text.data() + text.size()) {
    throw std::invalid_argument("last received id must be an integer");
  }
  return id;
}

}  // namespace

int main(const int argc, char* argv[]) {
  if (argc > 6) {
    std::cerr << "usage: news_client [address] [port] [username] [password] "
                 "[last_received_id]\n";
    return 2;
  }

  try {
    std::string address(kDefaultAddress);
    std::uint16_t port = kDefaultPort;
    std::string username(kDefaultUsername);
    std::string password(kDefaultPassword);
    std::uint64_t last_received_id = kDefaultLastReceivedId;

    if (argc >= 2) {
      address = argv[1];
    }
    if (argc >= 3) {
      port = ParsePort(argv[2]);
    }
    if (argc >= 4) {
      username = argv[3];
    }
    if (argc >= 5) {
      password = argv[4];
    }
    if (argc == 6) {
      last_received_id = ParseId(argv[5]);
    }

    news::NewsClient client(address, port, last_received_id);
    client.Run(username, password);
  } catch (const std::exception& error) {
    std::cerr << "news_client: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
