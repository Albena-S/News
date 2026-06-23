#ifndef NEWS_INCLUDE_NEWS_WAL_H_
#define NEWS_INCLUDE_NEWS_WAL_H_

#include <string>
#include <vector>

#include "news/news_record.h"

namespace news {

class Wal {
 public:
  Wal(std::string path);

  bool Append(const NewsRecord& record) const;
  std::vector<NewsRecord> Recover() const;

 private:
  std::string path_;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_WAL_H_
