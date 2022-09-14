#include "distributed/store/common/promise.h"

using namespace std;

Promise::Promise()
{
    done = false;
    reply = 0;
    timeout = 1000;
}

Promise::Promise(int timeoutMS)
{
    done = false;
    reply = 0;
    timeout = timeoutMS;
}

Promise::~Promise() { }

// Get configured timeout, return after this period
int
Promise::GetTimeout()
{
    return timeout;
}

// Functions for replying to the promise
void
Promise::ReplyInternal(int r)
{
    done = true;
    reply = r;
    cv.notify_all();
}

void
Promise::Reply(int r)
{
    lock_guard<mutex> l(lock);
    ReplyInternal(r);
}

void
Promise::Reply(int r, Timestamp t)
{
    lock_guard<mutex> l(lock);
    timestamp = t;
    ReplyInternal(r);
}

void
Promise::Reply(int r, string v)
{
    lock_guard<mutex> l(lock);
    value = v;
    ReplyInternal(r);
}

void
Promise::Reply(int r, Timestamp t, string v)
{
    lock_guard<mutex> l(lock);
    value = v;
    timestamp = t;
    ReplyInternal(r);
}

void
Promise::Reply(int r, std::vector<Timestamp> ts,
               std::map<std::string, std::string> vals,
               std::vector<std::string> keys,
               std::vector<uint64_t> blocks)
{
    lock_guard<mutex> l(lock);
    values.insert(vals.begin(), vals.end());
    for (auto t : ts) {
      timestamps.emplace_back(t);
    }
    for (auto& k : keys) {
      unverified_keys.emplace_back(k);
    }
    for (auto& b : blocks) {
      estimated_blocks.emplace_back(b);
    }
    ReplyInternal(r);
}

void
Promise::Reply(int r, VerifyStatus vs)
{
    lock_guard<mutex> l(lock);
    verify = vs;
    ReplyInternal(r);
}

void
Promise::Reply(int r, VerifyStatus vs, std::vector<std::string> keys,
    std::vector<uint64_t> blocks)
{
    lock_guard<mutex> l(lock);
    for (auto& k : keys) {
      unverified_keys.emplace_back(k);
    }
    for (auto& b : blocks) {
      estimated_blocks.emplace_back(b);
    }
    verify = vs;
    ReplyInternal(r);
}

// Functions for getting a reply from the promise
int
Promise::GetReply()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return reply;
}

VerifyStatus
Promise::GetVerifyStatus()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return verify;
}

Timestamp
Promise::GetTimestamp()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return timestamp;
}

string
Promise::GetValue()
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return value;
}

const std::map<std::string, std::string>& Promise::getValues() {
  unique_lock<mutex> l(lock);
  while(!done) {
        cv.wait(l);
    }
    return values;
}

Timestamp
Promise::GetTimestamp(size_t i)
{
    unique_lock<mutex> l(lock);
    while(!done) {
        cv.wait(l);
    }
    return timestamps[i];
}
