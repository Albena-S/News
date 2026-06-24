#ifndef NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_
#define NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_

#include <signal.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "news/authenticator.h"
#include "news/config.h"
#include "news/news_publisher.h"
#include "news/replay_ring.h"
#include "news/session.h"
#include "news/wal.h"

namespace news {

class EpollServer {
 public:
  /**
   * Creates the server, opens its listening socket, and registers all internal
   * event sources with epoll.
   */
  EpollServer(ServerConfig config);

  /**
   * Stops the publisher, closes all sessions, and closes owned file
   * descriptors.
   */
  ~EpollServer();

  /**
   * Runs the main epoll event loop until the server receives a stop signal.
   */
  void Run();

  /**
   * Returns the actual listening port.
   *
   * This is useful when the configured port is 0 and the operating system
   * chooses an available port.
   */
  std::uint16_t port() const;

 private:
  /**
   * Creates the non-blocking TCP listening socket, binds it to the configured
   * address and port, and starts listening for clients.
   */
  void CreateListener();

  /**
   * Creates the epoll instance used by the server event loop.
   */
  void CreateEpoll();

  /**
   * Creates a signal file descriptor for SIGINT and SIGTERM.
   *
   * This lets the server handle shutdown signals inside the epoll loop.
   */
  void CreateSignalFd();

  /**
   * Registers a file descriptor with epoll.
   *
   * @param fd The file descriptor to watch.
   * @param events The epoll event flags to watch for.
   */
  void AddToEpoll(int fd, std::uint32_t events);

  /**
   * Updates the epoll event flags for a client session.
   *
   * Write readiness is requested only when the session has queued data to send.
   */
  void UpdateSessionEvents(const Session& session);

  /**
   * Accepts all currently pending TCP clients from the listening socket.
   */
  void AcceptConnections();

  /**
   * Handles one epoll event for a connected client.
   *
   * @param fd The client socket file descriptor.
   * @param events The epoll event flags reported for this client.
   */
  void HandleSessionEvent(int fd, std::uint32_t events);

  /**
   * Decodes and handles the frame currently stored in a client receive buffer.
   */
  void HandleReceivedFrame(Session& session);

  /**
   * Reads the publisher notification and publishes all newly generated titles.
   */
  void HandlePublisherEvent();

  /**
   * Reads pending SIGINT or SIGTERM notifications and stops the server.
   */
  void HandleSignal();

  /**
   * Returns records starting at first_id.
   *
   * The in-memory replay ring is used when possible. Older records are read
   * from the WAL.
   */
  std::vector<NewsRecord> ReplayRecordsFrom(std::uint64_t first_id) const;

  /**
   * Converts one publisher title into a durable news record.
   *
   * The record is assigned an id, appended to the WAL, stored in the replay
   * ring, and broadcast to live subscribers.
   */
  void PublishTitle(const std::string& title);

  /**
   * Queues one already-created news record for every live client session.
   */
  void BroadcastNews(const NewsRecord& record);

  /**
   * Removes one client session from the server.
   */
  void CloseSession(int fd, const char* reason);

  /**
   * Stops owned resources and closes server file descriptors.
   */
  void CloseAll();

  ServerConfig config_;
  Authenticator authenticator_;
  Wal wal_;
  ReplayRing replay_ring_;
  NewsPublisher publisher_;
  std::unordered_map<int, Session> sessions_;
  std::uint64_t next_id_{1};
  int listener_fd_{-1};
  int epoll_fd_{-1};
  int signal_fd_{-1};
  std::uint16_t bound_port_{0};
  bool running_{false};
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_EPOLL_SERVER_H_
