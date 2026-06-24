#ifndef NEWS_INCLUDE_NEWS_WAL_H_
#define NEWS_INCLUDE_NEWS_WAL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "news/news_record.h"

namespace news {

/**
 * Append-only storage for published news records.
 *
 * The WAL lets the server rebuild previously published records after restart
 * and replay older records that are no longer in the in-memory replay ring.
 */
class Wal {
 public:
  Wal(std::string path);

  /**
   * Appends one record to the WAL file.
   */
  bool Append(const NewsRecord& record) const;

  /**
   * Reads all complete records from the WAL file.
   */
  std::vector<NewsRecord> Recover() const;

  /**
   * Reads records whose id is greater than or equal to first_id.
   */
  std::vector<NewsRecord> From(std::uint64_t first_id) const;

 private:
  std::string path_;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_WAL_H_
