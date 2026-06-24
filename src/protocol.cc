#include "news/protocol.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace news {
namespace {

void AppendUint16(std::vector<std::byte>& output, const std::uint16_t value) {
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::byte>(value & 0xffU));
}

void AppendUint32(std::vector<std::byte>& output, const std::uint32_t value) {
  output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::byte>(value & 0xffU));
}

void AppendUint64(std::vector<std::byte>& output, const std::uint64_t value) {
  for (int byte = 7; byte >= 0; --byte) {
    const auto shift = static_cast<unsigned int>(byte * 8);
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
  }
}

std::uint16_t ReadUint16(const std::vector<std::byte>& data,
                         const std::size_t offset) {
  return static_cast<std::uint16_t>(
      (std::to_integer<std::uint16_t>(data[offset]) << 8U) |
      std::to_integer<std::uint16_t>(data[offset + 1]));
}

std::uint32_t ReadUint32(const std::vector<std::byte>& data,
                         const std::size_t offset) {
  return (std::to_integer<std::uint32_t>(data[offset]) << 24U) |
         (std::to_integer<std::uint32_t>(data[offset + 1]) << 16U) |
         (std::to_integer<std::uint32_t>(data[offset + 2]) << 8U) |
         std::to_integer<std::uint32_t>(data[offset + 3]);
}

std::uint32_t ReadUint32(const std::vector<std::byte>& data) {
  return ReadUint32(data, 0);
}

std::uint64_t ReadUint64(const std::vector<std::byte>& data,
                         const std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index) {
    value <<= 8U;
    value |= std::to_integer<unsigned char>(data[offset + index]);
  }
  return value;
}

std::vector<std::byte> EncodeString(const std::string& text) {
  std::vector<std::byte> encoded;
  encoded.reserve(text.size());
  for (const auto character : text) {
    encoded.push_back(static_cast<std::byte>(character));
  }
  return encoded;
}

std::string DecodeString(const std::vector<std::byte>& data) {
  std::string decoded;
  decoded.reserve(data.size());
  for (const auto byte : data) {
    decoded.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return decoded;
}

}  // namespace

std::vector<std::byte> EncodeFrame(
    const MessageType type, const std::vector<std::byte>& payload) {
  std::vector<std::byte> encoded;
  encoded.reserve(kFrameHeaderBytes + payload.size());
  AppendUint32(encoded, static_cast<std::uint32_t>(payload.size()));
  AppendUint16(encoded, static_cast<std::uint16_t>(type));
  encoded.insert(encoded.end(), payload.begin(), payload.end());
  return encoded;
}

Frame DecodeFrame(const std::vector<std::byte>& data) {
  const auto payload_bytes = ReadUint32(data);
  const auto type = static_cast<MessageType>(ReadUint16(data, 4));
  const auto payload_begin =
      data.begin() + static_cast<std::ptrdiff_t>(kFrameHeaderBytes);
  const auto payload_end =
      payload_begin + static_cast<std::ptrdiff_t>(payload_bytes);

  return {type, {payload_begin, payload_end}};
}

std::vector<std::byte> EncodeAuthRequest(
    const std::string& username, const std::string& password) {
  return EncodeString(username + ':' + password);
}

Credentials DecodeAuthRequest(const std::vector<std::byte>& payload) {
  const auto text = DecodeString(payload);
  const auto separator = text.find(':');
  return {text.substr(0, separator), text.substr(separator + 1)};
}

std::vector<std::byte> EncodeAuthResult(const bool accepted) {
  return {accepted ? std::byte{1} : std::byte{0}};
}

bool DecodeAuthResult(const std::vector<std::byte>& payload) {
  return std::to_integer<unsigned int>(payload[0]) != 0U;
}

std::vector<std::byte> EncodeSubscribe(const std::uint64_t last_seen_id) {
  std::vector<std::byte> encoded;
  encoded.reserve(8);
  AppendUint64(encoded, last_seen_id);
  return encoded;
}

std::uint64_t DecodeSubscribe(const std::vector<std::byte>& payload) {
  return ReadUint64(payload, 0);
}

std::vector<std::byte> EncodeNews(const NewsRecord& record) {
  std::vector<std::byte> encoded;
  encoded.reserve(12 + record.title.size());
  AppendUint64(encoded, record.id);
  AppendUint32(encoded, static_cast<std::uint32_t>(record.title.size()));
  const auto title_bytes = EncodeString(record.title);
  encoded.insert(encoded.end(), title_bytes.begin(), title_bytes.end());
  return encoded;
}

NewsRecord DecodeNews(const std::vector<std::byte>& payload) {
  const auto id = ReadUint64(payload, 0);
  const auto title_size = ReadUint32(payload, 8);
  const auto title_begin =
      payload.begin() + static_cast<std::ptrdiff_t>(12);
  const auto title_end =
      title_begin + static_cast<std::ptrdiff_t>(title_size);
  return {id, DecodeString({title_begin, title_end})};
}

}  // namespace news
