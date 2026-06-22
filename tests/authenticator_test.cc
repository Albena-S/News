#include "news/authenticator.h"

#include <cassert>

int main() {
  news::Authenticator authenticator("config/users.conf.example");

  assert(news::Authenticator::HashPassword("demo") ==
         "a5e41b674276d396");
  assert(authenticator.Authenticate("demo", "demo"));
  assert(!authenticator.Authenticate("demo", "wrong"));
  assert(!authenticator.Authenticate("unknown", "demo"));

  return 0;
}
