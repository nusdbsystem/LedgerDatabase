#ifndef _VERSIONED_KV_STORE_H_
#define _VERSIONED_KV_STORE_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/store/common/timestamp.h"
#include "distributed/store/common/backend/type.h"
#include "ledger/ledgerdb/ledgerdb.h"
#include "ledger/qldb/qldb.h"
#include "ledger/qldb/bplus_config.h"
#include "ledger/sqlledger/sqlledger.h"
#include "distributed/proto/strong-proto.pb.h"

#include "tbb/concurrent_hash_map.h"
#include <set>
#include <map>
#include <vector>
#include <unordered_map>

class VersionedKVStore {
 public:
  VersionedKVStore();
  VersionedKVStore(const std::string& db_path, int timeout);
  ~VersionedKVStore();

  bool GetDigest(strongstore::proto::Reply* reply);

  bool BatchGet(const std::vector<std::string>& keys,
      strongstore::proto::Reply* reply);

  bool GetNVersions(std::vector<std::pair<std::string, size_t>>& ver_keys,
      strongstore::proto::Reply* reply);

  bool GetProof(const std::map<uint64_t, std::vector<std::string>>& keys,
                strongstore::proto::Reply* reply);

  bool GetProof(const uint64_t& seq,
                strongstore::proto::Reply* reply);

  bool GetRange(const std::string &start, const std::string &end,
                strongstore::proto::Reply* reply);

  void put(const std::vector<std::string> &keys,
           const std::vector<std::string> &values,
           const Timestamp &t,
           strongstore::proto::Reply* reply);

  bool get(const std::string &key,
           const Timestamp &t,
           std::pair<Timestamp, std::string> &value);

 private:

  std::unique_ptr<ledgebase::ledgerdb::LedgerDB> ldb;
  std::unique_ptr<ledgebase::qldb::QLDB> qldb_;
  std::unique_ptr<ledgebase::sqlledger::SQLLedger> sqlledger_;
};

#endif  /* _VERSIONED_KV_STORE_H_ */
