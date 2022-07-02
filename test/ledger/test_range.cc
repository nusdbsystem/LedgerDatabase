#include <string>
#include <vector>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "ledger/ledgerdb/ledgerdb.h"

TEST(LDB, range) {
  std::vector<std::string> keys, vals;

  ledgebase::ledgerdb::LedgerDB ldb(10, "testdb", "testledger");

  for (size_t i = 0; i < 100; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string val = "v" + std::to_string(i);
    keys.emplace_back(key);
    vals.emplace_back(val);
    if (keys.size() >= 10 || (i == 99 && keys.size() > 0)) {
      ldb.Set(keys, vals, 0);
      keys.clear();
      vals.clear();
    }
  }

  std::map<std::string, std::pair<uint64_t, std::pair<size_t, std::string>>> values;
  ldb.GetRange("k50", "k60", values);
  for (auto& v : values) {
    std::cout << v.first << ", " << v.second.second.second
              << " at block " << v.second.second.first
              << " (" << v.second.first << ")" << std::endl;
  }
}