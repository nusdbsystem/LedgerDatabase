#ifndef _BUFFER_CLIENT_H_
#define _BUFFER_CLIENT_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/store/common/promise.h"
#include "distributed/store/common/transaction.h"
#include "distributed/store/common/frontend/txnclient.h"

class BufferClient
{
public:
    BufferClient(TxnClient *txnclient);
    ~BufferClient();

    // Begin a transaction with given tid.
    void Begin(uint64_t tid);

    void GetNVersions(const string& key, size_t n, Promise *promise = NULL);

    void Get(const string &key, Promise *promise = NULL);

    void BufferKey(const std::string &key);

    void BatchGet(Promise *promise);

    void GetRange(const string &from, const string &to, Promise *promise);

    // Put value for given key.
    void Put(const string &key, const string &value, Promise *promise = NULL);

    // Prepare (Spanner requires a prepare timestamp)
    void Prepare(const Timestamp &timestamp = Timestamp(),
        Promise *promise = NULL); 

    // Commit the ongoing transaction.
    void Commit(uint64_t timestamp = 0, Promise *promise = NULL);

    // Abort the running transaction.
    void Abort(Promise *promise = NULL);

    bool Verify(const uint64_t block,
                const std::vector<std::string>& keys,
                Promise *promise = NULL);

    bool Audit(const uint64_t seq, Promise *promise = NULL);

private:
    // Underlying single shard transaction client implementation.
    TxnClient* txnclient;

    // Transaction to keep track of read and write set.
    Transaction txn;

    std::vector<std::pair<std::string, size_t>> history;

    // Unique transaction id to keep track of ongoing transaction.
    uint64_t tid;

    std::vector<std::string> buffer;
};

#endif /* _BUFFER_CLIENT_H_ */
