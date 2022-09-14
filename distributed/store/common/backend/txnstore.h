#ifndef _TXN_STORE_H_
#define _TXN_STORE_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/store/common/timestamp.h"
#include "distributed/store/common/transaction.h"
#include "distributed/proto/strong-proto.pb.h"
#include "distributed/store/common/backend/type.h"

class TxnStore
{
public:

    TxnStore();
    virtual ~TxnStore();

    virtual int BatchGet(uint64_t id,
                         const std::vector<std::string> &keys,    
                         strongstore::proto::Reply* reply);

    virtual int GetRange(const std::string &start, const std::string &end,
                         strongstore::proto::Reply* reply);

    virtual int Prepare(uint64_t id, const Transaction &txn);

    virtual void Abort(uint64_t id, const Transaction &txn = Transaction());

    virtual void Load(const std::vector<std::string> &keys,
                      const std::vector<std::string> &values,
                      const Timestamp &timestamp);

    virtual int GetProof(const std::map<uint64_t, std::vector<std::string>> &keys,
                         strongstore::proto::Reply* reply);

    virtual int GetProof(const uint64_t seq,
                          strongstore::proto::Reply* reply);

    virtual void Commit(uint64_t id, uint64_t timestamp,
                        std::vector<std::pair<std::string, size_t>> ver_keys,
                        strongstore::proto::Reply* reply);

    virtual void Commit(uint64_t id, uint64_t timestamp,
                        strongstore::proto::Reply* reply);
};

#endif /* _TXN_STORE_H_ */
