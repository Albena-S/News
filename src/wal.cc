#include "news/wal.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "binary_encoding.h"

namespace news {
namespace {

constexpr std::size_t kWalHeaderBytes = 12;

std::string EncodeRecord(const NewsRecord& record) {
  std::string encoded;
  encoded.reserve(kWalHeaderBytes + record.title.size());

  internal::AppendUint64(encoded, record.id);
  internal::AppendUint32(encoded,
                         static_cast<std::uint32_t>(record.title.size()));
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

    const auto id = internal::ReadUint64(header, 0);
    const auto title_size = internal::ReadUint32(header, 8);

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
