#include "news/wal.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace news {
namespace {

constexpr std::size_t kWalHeaderBytes = 12;

void AppendUint32(std::string& output, const std::uint32_t value) {
  output.push_back(static_cast<char>((value >> 24U) & 0xffU));
  output.push_back(static_cast<char>((value >> 16U) & 0xffU));
  output.push_back(static_cast<char>((value >> 8U) & 0xffU));
  output.push_back(static_cast<char>(value & 0xffU));
}

void AppendUint64(std::string& output, const std::uint64_t value) {
  for (int byte = 7; byte >= 0; --byte) {
    const auto shift = static_cast<unsigned int>(byte * 8);
    output.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

std::uint32_t ReadUint32(const std::string& input,
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

std::uint64_t ReadUint64(const std::string& input) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index) {
    value <<= 8U;
    value |= static_cast<unsigned char>(input[index]);
  }
  return value;
}

std::string EncodeRecord(const NewsRecord& record) {
  std::string encoded;
  encoded.reserve(kWalHeaderBytes + record.title.size());

  AppendUint64(encoded, record.id);
  AppendUint32(encoded, static_cast<std::uint32_t>(record.title.size()));
  encoded.append(record.title);
  return encoded;
}

}  // namespace

Wal::Wal(std::string path) : path_(std::move(path)) {}

bool Wal::Append(const NewsRecord& record) const {
  if (record.title.size() > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }

  std::ofstream file(path_, std::ios::binary | std::ios::app);
  if (!file) {
    return false;
  }

  const auto encoded = EncodeRecord(record);
  file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
  return file.good();
}

std::vector<NewsRecord> Wal::Recover() const {
  std::ifstream file(path_, std::ios::binary);
  if (!file) {
    return {};
  }

  std::vector<NewsRecord> records;
  for (;;) {
    std::string header(kWalHeaderBytes, '\0');
    file.read(header.data(), static_cast<std::streamsize>(header.size()));
    if (file.gcount() == 0) {
      break;
    }
    if (file.gcount() != static_cast<std::streamsize>(header.size())) {
      break;
    }

    const auto id = ReadUint64(header);
    const auto title_size = ReadUint32(header, 8);

    std::string title(title_size, '\0');
    file.read(title.data(), static_cast<std::streamsize>(title.size()));
    if (file.gcount() != static_cast<std::streamsize>(title.size())) {
      break;
    }

    records.push_back({id, title});
  }

  return records;
}

std::vector<NewsRecord> Wal::From(const std::uint64_t first_id) const {
  std::vector<NewsRecord> records;
  for (const auto& record : Recover()) {
    if (record.id >= first_id) {
      records.push_back(record);
    }
  }
  return records;
}

}  // namespace news
