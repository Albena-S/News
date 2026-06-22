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

}  // namespace news
