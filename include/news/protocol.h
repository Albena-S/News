#ifndef NEWS_INCLUDE_NEWS_PROTOCOL_H_
#define NEWS_INCLUDE_NEWS_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "news/news_record.h"

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

struct Credentials {
  std::string username;
  std::string password;
};

// Frame format: [4-byte payload length][2-byte message type][payload].
std::vector<std::byte> EncodeFrame(
    MessageType type, const std::vector<std::byte>& payload);

// This interview version assumes data contains one complete, valid frame.
Frame DecodeFrame(const std::vector<std::byte>& data);
std::size_t EncodedFrameSize(const std::vector<std::byte>& data,
                             std::size_t offset);
Frame DecodeFrameAt(const std::vector<std::byte>& data, std::size_t offset);

std::vector<std::byte> EncodeAuthRequest(
    const std::string& username, const std::string& password);
Credentials DecodeAuthRequest(const std::vector<std::byte>& payload);

std::vector<std::byte> EncodeAuthResult(bool accepted);
bool DecodeAuthResult(const std::vector<std::byte>& payload);

std::vector<std::byte> EncodeSubscribe(std::uint64_t last_seen_id);
std::uint64_t DecodeSubscribe(const std::vector<std::byte>& payload);

std::vector<std::byte> EncodeNews(const NewsRecord& record);
NewsRecord DecodeNews(const std::vector<std::byte>& payload);

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_PROTOCOL_H_
