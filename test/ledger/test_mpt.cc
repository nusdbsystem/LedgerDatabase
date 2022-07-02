#include <string>
#include <vector>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "ledger/common/db.h"
#include "ledger/ledgerdb/mpt/trie.h"

TEST(MPT, TPS) {
  std::vector<std::string> keys, vals;

  ledgebase::DB db;
  db.Open("testdb");

  for (size_t i = 0; i < 100000; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string val = "v" + std::to_string(i);
    keys.emplace_back(key);
    vals.emplace_back(val);
  }
  auto mpt = ledgebase::ledgerdb::Trie(&db, keys, vals);
  keys.clear();
  vals.clear();

  timeval t0, t1;
  gettimeofday(&t0, NULL);
  for (size_t i = 0; i < 100000; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string val = "vv" + std::to_string(i);
    keys.emplace_back(key);
    vals.emplace_back(val);
    if (keys.size() == 1000) {
      auto hash = mpt.Set(keys, vals).Clone();
      mpt = ledgebase::ledgerdb::Trie(&db, hash);
      keys.clear();
      vals.clear();
    }
  }
  gettimeofday(&t1, NULL);
  auto elapsed_time = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
  std::cerr << "# Latency: " << elapsed_time << std::endl;
}