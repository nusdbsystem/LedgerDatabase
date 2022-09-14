#ifndef _TXN_CLIENT_H_
#define _TXN_CLIENT_H_

#include "distributed/store/common/promise.h"
#include "distributed/store/common/timestamp.h"
#include "distributed/store/common/transaction.h"

#include <string>

#define DEFAULT_TIMEOUT_MS 250
#define DEFAULT_MULTICAST_TIMEOUT_MS 500

// Timeouts for various operations
#define GET_TIMEOUT 10000
#define GET_RETRIES 1
// Only used for QWStore
#define PUT_TIMEOUT 250
#define PREPARE_TIMEOUT 10000
#define PREPARE_RETRIES 1

#define COMMIT_TIMEOUT 10000
#define COMMIT_RETRIES 1

#define ABORT_TIMEOUT 1000
#define RETRY_TIMEOUT 500000

class TxnClient
{
public:
    TxnClient() {};
    virtual ~TxnClient() {};

    // Begin a transaction.
    virtual void Begin(uint64_t id) = 0;

    virtual bool Audit(const uint64_t& seq,
                       Promise* promise = NULL) = 0;

    virtual bool GetProof(const uint64_t block,
                          const std::vector<string>& keys,
                          Promise* promise = NULL) = 0;

    virtual void BatchGet(uint64_t tid,                                        
                          const std::vector<std::string> &keys,                
                          Promise *promise = NULL) = 0;

    virtual void GetRange(uint64_t tid, const std::string& from,
                          const std::string& to,
                          Promise *promise = NULL) = 0;

    // Prepare the transaction.
    virtual void Prepare(uint64_t id,
                         const Transaction &txn,
                         const Timestamp &timestamp = Timestamp(),
                         Promise *promise = NULL) = 0;

    virtual void Commit(uint64_t id,
        const Transaction &txn = Transaction(),
        const std::vector<std::pair<std::string, size_t> > &versions = {},
        uint64_t timestamp = 0,
        Promise *promise = NULL) = 0;
    
    // Abort all Get(s) and Put(s) since Begin().
    virtual void Abort(uint64_t id, 
                       const Transaction &txn = Transaction(), 
                       Promise *promise = NULL) = 0;
};

#endif /* _TXN_CLIENT_H_ */
