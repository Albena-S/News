#ifndef NEWS_SRC_BINARY_ENCODING_H_
#define NEWS_SRC_BINARY_ENCODING_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace news {
namespace internal {

inline void AppendUint16(std::vector<std::byte>& output,
                         const std::uint16_t value) {
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::byte>(value & 0xffU));
}

inline void AppendUint32(std::vector<std::byte>& output,
                         const std::uint32_t value) {
  output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::byte>(value & 0xffU));
}

inline void AppendUint64(std::vector<std::byte>& output,
                         const std::uint64_t value) {
  for (int byte = 7; byte >= 0; --byte) {
    const auto shift = static_cast<unsigned int>(byte * 8);
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
  }
}

inline std::uint16_t ReadUint16(const std::vector<std::byte>& input,
                                const std::size_t offset) {
  return static_cast<std::uint16_t>(
      (std::to_integer<std::uint16_t>(input[offset]) << 8U) |
      std::to_integer<std::uint16_t>(input[offset + 1]));
}

inline std::uint32_t ReadUint32(const std::vector<std::byte>& input,
                                const std::size_t offset) {
  return (std::to_integer<std::uint32_t>(input[offset]) << 24U) |
         (std::to_integer<std::uint32_t>(input[offset + 1]) << 16U) |
         (std::to_integer<std::uint32_t>(input[offset + 2]) << 8U) |
         std::to_integer<std::uint32_t>(input[offset + 3]);
}

inline std::uint64_t ReadUint64(const std::vector<std::byte>& input,
                                const std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index) {
    value <<= 8U;
    value |= std::to_integer<unsigned char>(input[offset + index]);
  }
  return value;
}

inline void AppendUint32(std::string& output, const std::uint32_t value) {
  output.push_back(static_cast<char>((value >> 24U) & 0xffU));
  output.push_back(static_cast<char>((value >> 16U) & 0xffU));
  output.push_back(static_cast<char>((value >> 8U) & 0xffU));
  output.push_back(static_cast<char>(value & 0xffU));
}

inline void AppendUint64(std::string& output, const std::uint64_t value) {
  for (int byte = 7; byte >= 0; --byte) {
    const auto shift = static_cast<unsigned int>(byte * 8);
    output.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

inline std::uint32_t ReadUint32(const std::string& input,
                                const std::size_t offset) {
  return (static_cast<std::uint32_t>(
              static_cast<unsigned char>(input[offset]))
          << 24U) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(input[offset + 1]))
          << 16U) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(input[offset + 2]))
          << 8U) |
         static_cast<std::uint32_t>(
             static_cast<unsigned char>(input[offset + 3]));
}

inline std::uint64_t ReadUint64(const std::string& input,
                                const std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index) {
    value <<= 8U;
    value |= static_cast<unsigned char>(input[offset + index]);
  }
  return value;
}

}  // namespace internal
}  // namespace news

#endif  // NEWS_SRC_BINARY_ENCODING_H_
