#include "news/epoll_server.h"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

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
    std::cerr << "usage: news_server [port] [bind_address]\n";
    return 2;
  }

  try {
    news::ServerConfig config;
    if (argc >= 2) {
      config.port = ParsePort(argv[1]);
    }
    if (argc == 3) {
      config.bind_address = argv[2];
    }

    news::EpollServer server(std::move(config));
    server.Run();
  } catch (const std::exception& error) {
    std::cerr << "news_server: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
