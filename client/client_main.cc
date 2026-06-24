#include "news_client.h"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

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

}  // namespace

int main(const int argc, char* argv[]) {
  if (argc > 3) {
    std::cerr << "usage: news_client [address] [port]\n";
    return 2;
  }

  try {
    std::string address = "127.0.0.1";
    std::uint16_t port = 9000;

    if (argc >= 2) {
      address = argv[1];
    }
    if (argc == 3) {
      port = ParsePort(argv[2]);
    }

    news::NewsClient client(address, port);
    client.Run("demo", "demo");
  } catch (const std::exception& error) {
    std::cerr << "news_client: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
