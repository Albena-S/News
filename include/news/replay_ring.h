#ifndef NEWS_INCLUDE_NEWS_REPLAY_RING_H_
#define NEWS_INCLUDE_NEWS_REPLAY_RING_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "news/news_record.h"

namespace news {

/**
 * Fixed-size in-memory cache of the most recent news records.
 *
 * It gives reconnecting clients a fast way to receive recent missed records
 * without reading the WAL when the requested id is still in memory.
 */
class ReplayRing {
 public:
  ReplayRing(std::size_t capacity);

  void Append(NewsRecord record);
  std::vector<NewsRecord> From(std::uint64_t first_id) const;

  std::uint64_t oldest_id() const;
  std::uint64_t newest_id() const;

 private:
  std::size_t capacity_{0};
  std::vector<NewsRecord> records_;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_REPLAY_RING_H_
