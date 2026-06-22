#ifndef NEWS_INCLUDE_NEWS_NEWS_RECORD_H_
#define NEWS_INCLUDE_NEWS_NEWS_RECORD_H_

#include <cstdint>
#include <string>

namespace news {

struct NewsRecord {
  std::uint64_t id{0};
  std::string title;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_NEWS_RECORD_H_
