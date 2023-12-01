#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "ledger/ledgerdb/ledgerdb.h"
#include "ledger/sqlledger/sqlledger.h"
#include "ledger/qldb/qldb.h"
#include "ledger/qldb/bplus_config.h"

void millisleep(size_t t) {
  timespec req;
  req.tv_sec = (int) t/1000; 
  req.tv_nsec = (int64_t)((t%1000)*1e6); 
  nanosleep(&req, NULL); 
}

std::string NextValue(int n) {
  std::string res;
  for (int i = 0; i < n; ++i) {
    res += rand() % 10 + 48;
  }
  return res;
}

TEST(LDB, storage) {

  ledgebase::ledgerdb::LedgerDB ldb(0, "testdb", "testledger");
  size_t repeat = 160000;
  for (size_t i = 0; i < repeat; ++i) {
    std::vector<std::string> keys, vals;
    std::string key = "k" + std::to_string(i);
    std::string val = "v" + std::to_string(i);
    keys.emplace_back(key);
    vals.emplace_back(val);
    ldb.Set(keys, vals, 0);

    if (i % 1000 == 999 ) {
      ldb.buildTree(0);
      millisleep(1000);
    }
  }
  millisleep(1000);

  std::cout << ldb.size() << std::endl;
}

TEST(SQL, storage) {
  ledgebase::qldb::BPlusConfig::Init(45, 15);
  ledgebase::sqlledger::SQLLedger sql(0, "testdb");
  size_t repeat = 160000;
  for (size_t i = 0; i < repeat; ++i) {
    std::vector<std::string> keys, vals;
    std::string key = "k" + std::to_string(i);
    std::string val = "v" + std::to_string(i);
    keys.emplace_back(key);
    vals.emplace_back(val);
    sql.Set(keys, vals);

    if (i % 1000 == 999 ) {
      sql.updateLedger(0);
      millisleep(1000);
    }
  }
  millisleep(1000);

  std::cout << sql.size() << std::endl;
}

TEST(QLDB, storage) {
  ledgebase::qldb::BPlusConfig::Init(45, 8);
  ledgebase::qldb::QLDB qldb("testdb");
  size_t repeat = 160000;
  for (size_t i = 0; i < repeat; ++i) {
    std::vector<std::string> keys, vals;
    std::string key = NextValue(10);
    std::string val = NextValue(256);
    keys.emplace_back(key);
    vals.emplace_back(val);
    qldb.Set("test", keys, vals);
  }

  std::cout << qldb.size() << std::endl;
}
