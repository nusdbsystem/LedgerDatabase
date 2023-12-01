#ifndef QLDB_H_
#define QLDB_H_

#include "ledger/common/chunk.h"
#include "ledger/common/db.h"
#include "ledger/common/hash.h"
#include "ledger/common/slice.h"
#include "ledger/qldb/qlnode.h"
#include "ledger/qldb/ql_btree.h"

namespace ledgebase {

namespace qldb {

class QLProofResult {
 public:
  Data data;
  BlockAddress addr;
  MetaData meta;
  std::vector<std::string> proof;
  std::vector<size_t> pos;

  bool Verify(const ledgebase::Hash digest);
};

struct DigestInfo {
  uint64_t tip;
  std::string digest;
};

class QLDB {
 public:
  QLDB(std::string dbpath);
  ~QLDB() = default;

  void CreateLedger(const std::string& name);

  Slice GetData(const std::string& name,
      const std::string& key) const;
  
  Chunk GetCommitted(const std::string& name,
      const std::string& key) const;

  std::map<std::string, std::string> Range(const std::string& name,
      const std::string& from, const std::string& to) const;
  
  std::vector<Chunk> GetHistory(const std::string& name,
      const std::string& key, size_t n) const;
  
  bool Set(const std::string& name,
           const std::vector<std::string>& keys,
           const std::vector<std::string>& vals);
  
  bool Delete(const std::string& name,
              const std::vector<std::string>& keys) const;
  
  DigestInfo digest(const std::string& name);

  QLProofResult getProof(const std::string& name, const uint64_t tip,
      const uint64_t block_addr, const uint32_t seq);

  size_t size() { return db_.size(); }

 protected:
  Chunk GetVersion(const std::string& name, const std::string& key,
      const size_t version) const;

  Hash calculateBlockHash(const std::vector<Hash>& proof,
      const std::string& name, const uint64_t seqno);

  void calculateDigest(const Hash& block_hash, const Hash& prev_hash,
      const std::string& name, const uint64_t seqno);

  DB db_;
  std::unique_ptr<QLBTree> indexed_;
  std::unique_ptr<QLBTree> history_;
};

} // namespace qldb

} // namespace ledgebase

#endif // QLDB_H_