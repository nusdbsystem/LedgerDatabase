#ifndef _STRONG_SHARDCLIENT_H_
#define _STRONG_SHARDCLIENT_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/lib/transport.h"
#include "distributed/replication/vr/client.h"
#include "distributed/store/common/frontend/client.h"
#include "distributed/store/common/frontend/txnclient.h"
#include "distributed/store/common/timestamp.h"
#include "distributed/store/common/transaction.h"
#include "ledger/common/db.h"
#include "ledger/ledgerdb/mpt/trie.h"
#include "ledger/ledgerdb/ledgerdb.h"
#include "ledger/qldb/qldb.h"
#include "ledger/sqlledger/sqlledger.h"
#include "distributed/proto/strong-proto.pb.h"

namespace strongstore {

enum Mode {
    MODE_UNKNOWN,
    MODE_OCC,
    MODE_LOCK,
    MODE_SPAN_OCC,
    MODE_SPAN_LOCK
};

class ShardClient : public TxnClient
{
public:
    /* Constructor needs path to shard config. */
    ShardClient(Mode mode,
        const std::string &configPath, 
        Transport *transport,
        uint64_t client_id,
        int shard,
        int closestReplica);
    ~ShardClient();

    // Overriding from TxnClient
    void Begin(uint64_t id) override;
    void BatchGet(uint64_t tid, const std::vector<std::string> &keys,
                  Promise *promise = NULL) override;
    void GetRange(uint64_t tid, const std::string& from, const std::string& to,
                  Promise *promise = NULL) override;
    bool GetProof(const uint64_t block,
                  const std::vector<string>& keys,
                  Promise* promise = NULL);
    bool Audit(const uint64_t& seq,
               Promise* promise = NULL);
    void Prepare(uint64_t id, 
                 const Transaction &txn,
                 const Timestamp &timestamp = Timestamp(),
                 Promise *promise = NULL) override;
    void Commit(uint64_t id,
        const Transaction &txn,
        const std::vector<std::pair<std::string, size_t>> &versionedKeys,
        uint64_t timestamp,
        Promise *promise = NULL) override;
    void Abort(uint64_t id, 
               const Transaction &txn,
               Promise *promise = NULL) override;

private:
    Transport *transport; // Transport layer.
    uint64_t client_id; // Unique ID for this client.
    int shard; // which shard this client accesses
    int replica; // which replica to use for reads

    replication::vr::VRClient *client; // Client proxy.
    Promise *waiting; // waiting thread
    Promise *blockingBegin; // block until finished 
    std::map<size_t, Promise*> verifyPromise;
    uint64_t tip_block;
    long audit_block;
    size_t uid;
    ledgebase::DB db_;

    void GetProofCallback(size_t uid,
                          const std::vector<std::string>& keys,
                          const std::string& request_str,
                          const std::string& reply_str);
    void AuditCallback(uint64_t seq, size_t uid,
                       const std::string& request_str,
                       const std::string& reply_str);
    void GetTimeout();
    void BatchGetCallback(const std::string &, const std::string &);
    void GetRangeCallback(const std::string &, const std::string &);
    void PrepareCallback(const std::string &, const std::string &);
    void CommitCallback(const std::string &, const std::string &);
    void AbortCallback(const std::string &, const std::string &);

    /* Helper Functions for starting and finishing requests */
    void StartRequest();
    void WaitForResponse();
    void FinishRequest(const std::string &reply_str);
    void FinishRequest();
    int SendGet(const std::string &request_str);

};

} // namespace strongstore

#endif /* _STRONG_SHARDCLIENT_H_ */
