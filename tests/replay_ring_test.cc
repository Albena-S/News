#include "news/replay_ring.h"

#include <cassert>

void TestAppendAndReadFromSequence() {
  news::ReplayRing ring(3);
  ring.Append({1, "one"});
  ring.Append({2, "two"});
  ring.Append({3, "three"});

  assert(ring.oldest_id() == 1);
  assert(ring.newest_id() == 3);

  const auto records = ring.From(2);
  assert(records.size() == 2);
  assert(records[0].id == 2);
  assert(records[0].title == "two");
  assert(records[1].id == 3);
  assert(records[1].title == "three");
}

void TestOldRecordsAreDropped() {
  news::ReplayRing ring(2);
  ring.Append({1, "one"});
  ring.Append({2, "two"});
  ring.Append({3, "three"});

  assert(ring.oldest_id() == 2);
  assert(ring.newest_id() == 3);

  const auto records = ring.From(1);
  assert(records.size() == 2);
  assert(records[0].id == 2);
  assert(records[1].id == 3);
}

int main() {
  TestAppendAndReadFromSequence();
  TestOldRecordsAreDropped();

  return 0;
}
