#include "distributed/store/common/frontend/bufferclient.h"

using namespace std;

BufferClient::BufferClient(TxnClient* txnclient) : txn()
{
    this->txnclient = txnclient;
}

BufferClient::~BufferClient() { }

/* Begins a transaction. */
void
BufferClient::Begin(uint64_t tid)
{
    // Initialize data structures.
    txn = Transaction();
    this->tid = tid;
    txnclient->Begin(tid);
    history.clear();
    buffer.clear();
}

void
BufferClient::GetNVersions(const string &key, size_t n, Promise* promise)
{
    history.emplace_back(std::make_pair(key, n));
    promise->Reply(REPLY_OK);
}

void
BufferClient::BatchGet(Promise *promise)
{
    // Read your own writes, check the write set first.
    txnclient->BatchGet(tid, buffer, promise);
    if (promise->GetReply() == REPLY_OK) {
        size_t count = 0;
        for (auto& key : promise->getValues()) {
            txn.addReadSet(key.first, promise->GetTimestamp(count));
            ++count;
        }
    }
    buffer.clear();
}

void BufferClient::GetRange(const string &from, const string &to,
                            Promise *promise)
{
  txnclient->GetRange(tid, from, to, promise);
  if (promise->GetReply() == REPLY_OK) {
    size_t count = 0;
    for (auto& entry : promise->getValues()) {
      txn.addReadSet(entry.first, promise->GetTimestamp(count));
      ++count;
    }
  }
}

void
BufferClient::BufferKey(const std::string &key)
{
    // Read your own writes, check the write set first.
    buffer.emplace_back(key);
}

void
BufferClient::Get(const string &key, Promise* promise)
{
    // Read your own writes, check the write set first.
    if (txn.getWriteSet().find(key) != txn.getWriteSet().end()) {
        promise->Reply(REPLY_OK);
        return;
    }

    if (txn.getReadSet().find(key) == txn.getReadSet().end()) {
        txn.addReadSet(key, Timestamp());
    }
    promise->Reply(REPLY_OK);
}

/* Set value for a key. (Always succeeds).
 * Returns 0 on success, else -1. */
void
BufferClient::Put(const string &key, const string &value, Promise *promise)
{
    // Update the write set.
    txn.addWriteSet(key, value);
    promise->Reply(REPLY_OK);
}

/* Prepare the transaction. */
void
BufferClient::Prepare(const Timestamp &timestamp, Promise *promise)
{
    txnclient->Prepare(tid, txn, timestamp, promise);
}

void
BufferClient::Commit(uint64_t timestamp, Promise *promise)
{
    txnclient->Commit(tid, txn, history, timestamp, promise);
}

/* Aborts the ongoing transaction. */
void
BufferClient::Abort(Promise *promise)
{
    txnclient->Abort(tid, Transaction(), promise);
}

bool BufferClient::Verify(const uint64_t block,
                          const std::vector<std::string>& keys,
                          Promise *promise) {
    return txnclient->GetProof(block, keys, promise);
}

bool BufferClient::Audit(const uint64_t seq,
                         Promise *promise) {
    return txnclient->Audit(seq, promise);
 }