#ifndef NEWS_INCLUDE_NEWS_NEWS_PUBLISHER_H_
#define NEWS_INCLUDE_NEWS_NEWS_PUBLISHER_H_

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace news {

class NewsPublisher {
 public:
  NewsPublisher();
  ~NewsPublisher();

  void Start();
  void Stop();

  int event_fd() const;
  std::vector<std::string> TakeTitles();

 private:
  std::string NextTitle();
  void Run();
  void NotifyServer() const;

  int event_fd_{-1};
  std::thread thread_;
  mutable std::mutex mutex_;
  std::condition_variable stop_cv_;
  std::queue<std::string> titles_;
  bool running_{false};
  std::size_t title_index_{0};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_NEWS_PUBLISHER_H_
