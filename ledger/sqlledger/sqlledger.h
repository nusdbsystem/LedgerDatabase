#ifndef SQLLEDGER_H_
#define SQLLEDGER_H_

#include <thread>
#include "boost/thread.hpp"
#include "ledger/common/db.h"
#include "ledger/common/logger.h"
#include "ledger/qldb/ql_btree.h"
#include "ledger/sqlledger/sqlldg_mt.h"

namespace ledgebase {

namespace sqlledger {

struct BlockProof {
  std::vector<std::string> blks;

  bool Verify(const std::string& hash, std::string* mt_blk_root);
};

struct DetailProof {
  Proof txn_proof;
  Proof data_proof;

  bool Verify(const std::string& Hash);
};

struct Auditor {
  std::vector<std::string> blks;
  std::string txns;
  std::string digest;
  uint64_t block_seq;

  bool Audit(DB* db, size_t* ntxn);
};

class SQLLedger {
 public:
  SQLLedger(int t, const std::string& dbpath);
  ~SQLLedger() = default;

  void updateLedger(int timeout);

  void GetDigest(uint64_t* tip, std::string* hash);
  
  uint64_t Set(const std::vector<std::string>& keys,
               const std::vector<std::string>& vals);

  std::string GetCommitted(const std::string& key) const;

  std::string GetDataAtBlock(const std::string& key,
      const uint64_t& block_seq);

  std::map<std::string, std::string> Range(const std::string& from,
      const std::string& to);
  
  std::vector<std::string> GetHistory(const std::string& key, size_t n);

  BlockProof getBlockProof(const uint64_t block_addr, const uint64_t tip, 
      int* level);

  DetailProof getDetailProof(const std::string& key, const uint64_t block_addr,
      int level);

  Auditor getAudit(const uint64_t& seq);

  size_t size() { return db_.size(); }

 private:

  Logger logger_;
  DB db_;
  std::unique_ptr<std::thread> buildThread_;
  std::unique_ptr<MerkleTree> mt_;
  //std::vector<std::string> *buffer_;
  std::unique_ptr<std::map<uint64_t, std::vector<std::string>>> buffer_;
  uint64_t block_seq_;
  uint64_t tid_;
  boost::shared_mutex lock_;
  boost::shared_mutex buffer_lock_;
  std::unique_ptr<qldb::QLBTree> indexed_;
  std::unique_ptr<qldb::QLBTree> history_;
  Hash dummy_;
};

}  // namespace sqlledger

}  // namespace ledgebase

#endif // SQLLEDGER_H_