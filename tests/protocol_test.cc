#include "news/protocol.h"

#include <cassert>
#include <cstddef>
#include <vector>

int main() {
  const auto auth_payload = news::EncodeAuthRequest(
      "demo", " password: with spaces ");
  const auto credentials = news::DecodeAuthRequest(auth_payload);
  assert(credentials.username == "demo");
  assert(credentials.password == " password: with spaces ");

  const auto accepted_payload = news::EncodeAuthResult(true);
  assert(news::DecodeAuthResult(accepted_payload));

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
