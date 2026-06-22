#include "news/protocol.h"

#include <cstddef>
#include <cstdint>

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

std::uint16_t ReadUint16(const std::vector<std::byte>& data,
                         const std::size_t offset) {
  return static_cast<std::uint16_t>(
      (std::to_integer<std::uint16_t>(data[offset]) << 8U) |
      std::to_integer<std::uint16_t>(data[offset + 1]));
}

std::uint32_t ReadUint32(const std::vector<std::byte>& data) {
  return (std::to_integer<std::uint32_t>(data[0]) << 24U) |
         (std::to_integer<std::uint32_t>(data[1]) << 16U) |
         (std::to_integer<std::uint32_t>(data[2]) << 8U) |
         std::to_integer<std::uint32_t>(data[3]);
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

}  // namespace news
