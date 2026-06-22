#ifndef NEWS_INCLUDE_NEWS_CONFIG_H_
#define NEWS_INCLUDE_NEWS_CONFIG_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "news/protocol.h"

namespace news {

struct ServerConfig {
  std::string bind_address{"0.0.0.0"};
  std::uint16_t port{9000};
  int listen_backlog{128};
  std::size_t receive_buffer_bytes{kMaxFrameBytes};
  std::size_t max_queued_bytes{1024U * 1024U};
  bool tcp_no_delay{true};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_CONFIG_H_
