#include "news/authenticator.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace news {
namespace {

constexpr std::uint64_t kHashInitialValue = 14695981039346656037ULL;
constexpr std::uint64_t kHashMultiplier = 1099511628211ULL;

}  // namespace

Authenticator::Authenticator(std::string users_file_path)
    : users_file_path_(std::move(users_file_path)) {
  LoadUsers();
}

bool Authenticator::Authenticate(
    const std::string& username, const std::string& password) const {
  const auto found = password_hash_by_user_.find(username);
  if (found == password_hash_by_user_.end()) {
    return false;
  }
  return found->second == HashPassword(password);
}

std::string Authenticator::HashPassword(const std::string& password) {
  auto hash = kHashInitialValue;
  for (const auto character : password) {
    hash ^= static_cast<unsigned char>(character);
    hash *= kHashMultiplier;
  }

  std::ostringstream output;
  output << std::hex << std::setw(16) << std::setfill('0') << hash;
  return output.str();
}

void Authenticator::LoadUsers() {
  std::ifstream users_file(users_file_path_);
  if (!users_file) {
    throw std::runtime_error("could not open users file");
  }

  std::string line;
  while (std::getline(users_file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      continue;
    }

    const auto username = line.substr(0, separator);
    const auto password_hash = line.substr(separator + 1);
    password_hash_by_user_[username] = password_hash;
  }
}

}  // namespace news
