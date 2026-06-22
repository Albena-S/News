#ifndef NEWS_INCLUDE_NEWS_PROTOCOL_H_
#define NEWS_INCLUDE_NEWS_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace news {

constexpr std::size_t kFrameHeaderBytes = 6;
constexpr std::size_t kMaxPayloadBytes = 64U * 1024U;
constexpr std::size_t kMaxFrameBytes =
    kFrameHeaderBytes + kMaxPayloadBytes;

enum class MessageType : std::uint16_t {
  kAuthRequest = 1,
  kAuthResult = 2,
  kSubscribe = 3,
  kNews = 4,
};

struct Frame {
  MessageType type;
  std::vector<std::byte> payload;
};

// Frame format: [4-byte payload length][2-byte message type][payload].
std::vector<std::byte> EncodeFrame(
    MessageType type, const std::vector<std::byte>& payload);

// This interview version assumes data contains one complete, valid frame.
Frame DecodeFrame(const std::vector<std::byte>& data);

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_PROTOCOL_H_
