#include "news/protocol.h"

#include <cassert>
#include <cstddef>
#include <vector>

int main() {
  const std::vector<std::byte> payload{
      std::byte{'n'},
      std::byte{'e'},
      std::byte{'w'},
      std::byte{'s'},
  };

  const auto encoded = news::EncodeFrame(news::MessageType::kNews, payload);
  const auto decoded = news::DecodeFrame(encoded);

  assert(decoded.type == news::MessageType::kNews);
  assert(decoded.payload == payload);
  return 0;
}
