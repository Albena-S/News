#ifndef NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_
#define NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_

#include <string>
#include <unordered_map>

namespace news {

class Authenticator {
 public:
  Authenticator(std::string users_file_path);

  bool Authenticate(const std::string& username,
                    const std::string& password) const;

  static std::string HashPassword(const std::string& password);

 private:
  void LoadUsers();

  std::string users_file_path_;
  std::unordered_map<std::string, std::string> password_hash_by_user_;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_
