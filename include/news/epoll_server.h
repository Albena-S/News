#ifndef NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_
#define NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_

#include <signal.h>

#include <cstdint>
#include <unordered_map>

#include "news/authenticator.h"
#include "news/config.h"
#include "news/session.h"

namespace news {

class EpollServer {
 public:
  EpollServer(ServerConfig config);
  ~EpollServer();

  void Run();
  std::uint16_t port() const;

 private:
  void CreateListener();
  void CreateEpoll();
  void CreateSignalFd();
  void AddToEpoll(int fd, std::uint32_t events);
  void UpdateSessionEvents(const Session& session);

  void AcceptConnections();
  void HandleSessionEvent(int fd, std::uint32_t events);
  void HandleReceivedFrame(Session& session);
  void HandleSignal();
  void CloseSession(int fd, const char* reason);
  void CloseAll();

  ServerConfig config_;
  Authenticator authenticator_;
  std::unordered_map<int, Session> sessions_;
  int listener_fd_{-1};
  int epoll_fd_{-1};
  int signal_fd_{-1};
  std::uint16_t bound_port_{0};
  bool running_{false};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_
