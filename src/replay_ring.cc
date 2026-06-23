#include "news/replay_ring.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace news {

ReplayRing::ReplayRing(const std::size_t capacity) : capacity_(capacity) {
  records_.reserve(capacity_);
}

void ReplayRing::Append(NewsRecord record) {
  if (capacity_ == 0) {
    return;
  }

  records_.push_back(std::move(record));
  if (records_.size() > capacity_) {
    records_.erase(records_.begin());
  }
}

std::vector<NewsRecord> ReplayRing::From(
    const std::uint64_t first_id) const {
  std::vector<NewsRecord> result;
  for (const auto& record : records_) {
    if (record.id >= first_id) {
      result.push_back(record);
    }
  }
  return result;
}

std::uint64_t ReplayRing::oldest_id() const {
  if (records_.empty()) {
    return 0;
  }
  return records_.front().id;
}

std::uint64_t ReplayRing::newest_id() const {
  if (records_.empty()) {
    return 0;
  }
  return records_.back().id;
}

}  // namespace news
