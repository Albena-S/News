#include "news/wal.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string TestWalPath() {
  return (std::filesystem::temp_directory_path() / "news_wal_test.log")
      .string();
}

void RemoveTestWal() {
  std::filesystem::remove(TestWalPath());
}

void TestEmptyWal() {
  RemoveTestWal();

  const news::Wal wal(TestWalPath());
  assert(wal.Recover().empty());
}

void TestAppendAndRecover() {
  RemoveTestWal();

  const news::Wal wal(TestWalPath());
  assert(wal.Append({1, "first title"}));
  assert(wal.Append({2, "second title"}));
  assert(wal.Append({3, "third title"}));

  const auto records = wal.Recover();
  assert(records.size() == 3);
  assert(records[0].id == 1);
  assert(records[0].title == "first title");
  assert(records[1].id == 2);
  assert(records[1].title == "second title");
  assert(records[2].id == 3);
  assert(records[2].title == "third title");
}

void TestReadFromId() {
  RemoveTestWal();

  const news::Wal wal(TestWalPath());
  assert(wal.Append({1, "first title"}));
  assert(wal.Append({2, "second title"}));
  assert(wal.Append({3, "third title"}));

  const auto records = wal.From(2);
  assert(records.size() == 2);
  assert(records[0].id == 2);
  assert(records[0].title == "second title");
  assert(records[1].id == 3);
  assert(records[1].title == "third title");
}

void TestPartialFinalRecordIsIgnored() {
  RemoveTestWal();

  const news::Wal wal(TestWalPath());
  assert(wal.Append({1, "complete title"}));

  std::ofstream file(TestWalPath(), std::ios::binary | std::ios::app);
  file << "partial";
  file.close();

  const auto records = wal.Recover();
  assert(records.size() == 1);
  assert(records[0].id == 1);
  assert(records[0].title == "complete title");
}

}  // namespace

int main() {
  TestEmptyWal();
  TestAppendAndRecover();
  TestReadFromId();
  TestPartialFinalRecordIsIgnored();
  RemoveTestWal();

  return 0;
}
