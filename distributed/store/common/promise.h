#ifndef _PROMISE_H_
#define _PROMISE_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include "distributed/lib/transport.h"
#include "distributed/store/common/transaction.h"

#include <condition_variable>
#include <mutex>

enum VerifyStatus {
  PASS = 0,
  UNVERIFIED = 1,
  FAILED = 9
};

class Promise
{
private:
    bool done;
    int timeout;
    int reply;
    VerifyStatus verify;
    Timestamp timestamp;
    std::string value;
    std::mutex lock;
    std::condition_variable cv;
    std::vector<std::string> unverified_keys;
    std::vector<uint64_t> estimated_blocks;
    std::map<std::string, std::string> values;
    std::vector<Timestamp> timestamps;

    void ReplyInternal(int r);

public:
    Promise();
    Promise(int timeoutMS); // timeout in milliseconds
    ~Promise();

    // reply to this promise and unblock any waiting threads
    void Reply(int r);
    void Reply(int r, Timestamp t);
    void Reply(int r, std::string v);
    void Reply(int r, Timestamp t, std::string v);
    void Reply(int r, std::vector<Timestamp> ts,
               std::map<std::string, std::string> vs,
               std::vector<std::string> keys,
               std::vector<uint64_t> blocks);

    void Reply(int r, VerifyStatus vs);
    void Reply(int r, VerifyStatus vs, std::vector<std::string> keys,
        std::vector<uint64_t> blocks);

    // Return configured timeout
    int GetTimeout();

    // block on this until response comes back
    int GetReply();

    VerifyStatus GetVerifyStatus();
    
    Timestamp GetTimestamp();
    std::string GetValue();

    Timestamp GetTimestamp(size_t i);
    const std::map<std::string, std::string>& getValues();

    std::string GetUnverifiedKey(size_t i) const { return unverified_keys[i]; }
    size_t UnverifiedKeySize() const { return unverified_keys.size(); }
    uint64_t GetEstimateBlock(size_t i) const { return estimated_blocks[i]; }
    size_t EstimateBlockSize() const {return estimated_blocks.size(); }
};

#endif /* _PROMISE_H_ */
