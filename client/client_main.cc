#include "news_client.h"

int main() {
  news::NewsClient client("127.0.0.1", 9000);

  if (!client.Connect()) {
    return 1;
  }
  if (!client.Authenticate("demo", "demo")) {
    return 1;
  }
  if (!client.Subscribe(0)) {
    return 1;
  }

  client.ReceiveNews();
  return 0;
}
