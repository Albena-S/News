#include "news/news_publisher.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace news {
namespace {

constexpr std::chrono::seconds kPublishInterval{20};
constexpr std::uint64_t kWakeServer = 1;

const std::array<std::string, 4> kDemoTitles{
    "Breaking: market opens higher",
    "Weather: rain expected tomorrow",
    "Sports: local team wins",
    "Tech: new chip announced",
};

}  // namespace

NewsPublisher::NewsPublisher() {
  event_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (event_fd_ < 0) {
    throw std::system_error(errno, std::generic_category(), "eventfd");
  }
}

NewsPublisher::~NewsPublisher() {
  Stop();
  if (event_fd_ >= 0) {
    ::close(event_fd_);
    event_fd_ = -1;
  }
}

void NewsPublisher::Start() {
  {
    std::lock_guard lock(mutex_);
    if (running_) {
      return;
    }
    running_ = true;
  }

  thread_ = std::thread(&NewsPublisher::Run, this);
}

void NewsPublisher::Stop() {
  {
    std::lock_guard lock(mutex_);
    if (!running_) {
      return;
    }
    running_ = false;
  }

  stop_cv_.notify_one();
  if (thread_.joinable()) {
    thread_.join();
  }
}

int NewsPublisher::event_fd() const {
  return event_fd_;
}

std::vector<std::string> NewsPublisher::TakeTitles() {
  std::vector<std::string> titles;
  std::lock_guard lock(mutex_);

  while (!titles_.empty()) {
    titles.push_back(std::move(titles_.front()));
    titles_.pop();
  }

  return titles;
}

std::string NewsPublisher::NextTitle() {
  const auto title = kDemoTitles[title_index_];
  title_index_ = (title_index_ + 1) % kDemoTitles.size();
  return title;
}

void NewsPublisher::Run() {
  for (;;) {
    std::unique_lock lock(mutex_);
    if (stop_cv_.wait_for(lock, kPublishInterval,
                          [this] { return !running_; })) {
      return;
    }

    titles_.push(NextTitle());
    lock.unlock();

    NotifyServer();
  }
}

void NewsPublisher::NotifyServer() const {
  for (;;) {
    const auto written = ::write(event_fd_, &kWakeServer, sizeof(kWakeServer));
    if (written == static_cast<ssize_t>(sizeof(kWakeServer))) {
      return;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    return;
  }
}

}  // namespace news
