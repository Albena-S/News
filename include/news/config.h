#ifndef NEWS_INCLUDE_NEWS_CONFIG_H_
#define NEWS_INCLUDE_NEWS_CONFIG_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "news/protocol.h"

namespace news {

struct ServerConfig {
  std::string bind_address{"127.0.0.1"};
  std::uint16_t port{9000};
  int listen_backlog{128};
  std::size_t receive_buffer_bytes{kMaxFrameBytes};
  std::size_t max_queued_bytes{1024U * 1024U};
  std::string users_file_path{"config/users.conf.example"};
  std::string wal_file_path{"news.wal"};
  std::size_t replay_ring_capacity{128};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_CONFIG_H_
