#ifndef NEWS_CLIENT_NEWS_CLIENT_H_
#define NEWS_CLIENT_NEWS_CLIENT_H_

#include <cstdint>
#include <string>

namespace news {

class NewsClient {
 public:
  NewsClient(std::string address, std::uint16_t port);
  ~NewsClient();

  bool Connect();
  bool Authenticate(const std::string& username, const std::string& password);
  bool Subscribe(std::uint64_t last_seen_id);
  void ReceiveNews();
  void Run(const std::string& username, const std::string& password);

 private:
  void Close();

  std::string address_;
  std::uint16_t port_;
  std::uint64_t last_received_id_{0};
  int socket_fd_{-1};
};

}  // namespace news

#endif  // NEWS_CLIENT_NEWS_CLIENT_H_
