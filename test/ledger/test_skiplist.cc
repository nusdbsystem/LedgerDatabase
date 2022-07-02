#include <string>
#include <vector>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "ledger/common/db.h"
#include "ledger/ledgerdb/skiplist/skiplist.h"

TEST(skiplist, test) {
  ledgebase::DB db;
  db.Open("testdb");
  ledgebase::ledgerdb::SkipList sl(&db);

  for (size_t i = 0; i < 100; ++i) {
    for (size_t j = 0; j < 10; ++j) {
      sl.insert("test_" + std::to_string(i), j, "value" + std::to_string(i) + "_" + std::to_string(j));
    }
  }

  for (size_t i = 0; i < 100; ++i) {
    for (size_t j = 0; j < 10; ++j) {
      auto node = sl.find("test_" + std::to_string(i), j);
      ledgebase::ledgerdb::SkipNode sn(node);
      ASSERT_EQ(sn.value, "value" + std::to_string(i) + "_" + std::to_string(j));
    }
  }

  std::vector<std::string> nodes;
  sl.scan("test_0", 10, nodes);
  for (auto& node : nodes) {
    std::cout << node << std::endl;
  }
}