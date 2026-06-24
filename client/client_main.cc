#include "news_client.h"

int main() {
  news::NewsClient client("127.0.0.1", 9000);

  client.Run("demo", "demo");
  return 0;
}
