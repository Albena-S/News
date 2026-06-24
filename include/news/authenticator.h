#ifndef NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_
#define NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_

#include <string>
#include <unordered_map>

namespace news {

/**
 * Loads demo users and checks username/password pairs.
 *
 * Passwords are hashed with the same simple deterministic hash used in the
 * users file. This is enough for the demo, but it is not production password
 * storage.
 */
class Authenticator {
 public:
  Authenticator(std::string users_file_path);

  bool Authenticate(const std::string& username,
                    const std::string& password) const;

  /**
   * Hashes a password into a fixed hexadecimal string.
   *
   * The hash starts from a fixed initial value, mixes each password byte into
   * the value, and multiplies by a fixed multiplier after every byte.
   */
  static std::string HashPassword(const std::string& password);

 private:
  void LoadUsers();

  std::string users_file_path_;
  std::unordered_map<std::string, std::string> password_hash_by_user_;
};

}  // namespace news

#endif  // NEWS_INCLUDE_NEWS_AUTHENTICATOR_H_
