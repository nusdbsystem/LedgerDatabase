#ifndef _STRONG_OCC_STORE_H_
#define _STRONG_OCC_STORE_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/store/common/backend/versionstore.h"
#include "distributed/store/common/backend/txnstore.h"
#include "distributed/store/common/transaction.h"

#include <map>
#include <vector>

namespace strongstore {

class OCCStore : public TxnStore
{
public:
    OCCStore(const std::string& db_path, int timeout);
    ~OCCStore();

    int BatchGet(uint64_t id,
                 const std::vector<std::string> &keys,    
                 strongstore::proto::Reply* reply);
    
    int GetRange(const std::string &start, const std::string &end,
                 strongstore::proto::Reply* reply);

    int Prepare(uint64_t id, const Transaction &txn);

    void Abort(uint64_t id, const Transaction &txn = Transaction());

    void Load(const std::vector<std::string> &keys,
              const std::vector<std::string> &values,
              const Timestamp &timestamp);

    void Commit(uint64_t id, uint64_t timestamp,
                std::vector<std::pair<std::string, size_t>> ver_keys,
                strongstore::proto::Reply* reply);

    void Commit(uint64_t id, uint64_t timestamp,
                strongstore::proto::Reply* reply);

    int GetProof(const std::map<uint64_t, std::vector<std::string>> &keys,
                 strongstore::proto::Reply* reply);

    int GetProof(const uint64_t seq,
                  strongstore::proto::Reply* reply);

private:
    // Data store.
    VersionedKVStore store;

    std::map<uint64_t, Transaction> prepared;

    std::set<std::string> getPreparedWrites();
    std::set<std::string> getPreparedReadWrites();
};

} // namespace strongstore

#endif /* _STRONG_OCC_STORE_H_ */
