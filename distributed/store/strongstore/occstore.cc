#include "distributed/store/strongstore/occstore.h"

namespace strongstore {

using namespace std;

OCCStore::OCCStore(const std::string& db_path, int timeout) :
    store(db_path, timeout) { }
OCCStore::~OCCStore() { }

int OCCStore::BatchGet(uint64_t id, const std::vector<std::string> &keys,
                       strongstore::proto::Reply* reply) {
  store.GetDigest(reply);
  if (store.BatchGet(keys, reply)) {
    return REPLY_OK;
  } else {
    return REPLY_FAIL;
  }
}

int OCCStore::GetRange(const std::string& start, const std::string& end,
                       strongstore::proto::Reply* reply) {
  store.GetDigest(reply);
  if (store.GetRange(start, end, reply)) {
    return REPLY_OK;
  } else {
    return REPLY_FAIL;
  }
}

int
OCCStore::Prepare(uint64_t id, const Transaction &txn)
{

  if (prepared.find(id) != prepared.end()) {
    return REPLY_OK;
  }

  // Do OCC checks.
  set<string> pWrites = getPreparedWrites();
  set<string> pRW = getPreparedReadWrites();

  // Check for conflicts with the read set.
  for (auto &read : txn.getReadSet()) {
    // If there is a pending write for this key, abort.
    if (pWrites.find(read.first) != pWrites.end()) {
      Abort(id);
      return REPLY_FAIL;
    }
  }

  // Check for conflicts with the write set.
  for (auto &write : txn.getWriteSet()) {
    // If there is a pending read or write for this key, abort.
    if (pRW.find(write.first) != pRW.end()) {
      Abort(id);
      return REPLY_FAIL;
    }
  }

  // Otherwise, prepare this transaction for commit
  prepared[id] = txn;
  return REPLY_OK;
}

void
OCCStore::Abort(uint64_t id, const Transaction &txn)
{
  prepared.erase(id);
}

void
OCCStore::Load(const vector<string> &keys, const vector<string> &values,
        const Timestamp &timestamp)
{
  store.put(keys, values, timestamp, nullptr);
}

void
OCCStore::Commit(uint64_t id, uint64_t timestamp,
                 std::vector<std::pair<std::string, size_t>> ver_keys,
                 strongstore::proto::Reply* reply)
{
  ASSERT(prepared.find(id) != prepared.end());
  Transaction txn = prepared[id];

  store.GetDigest(reply);

  std::vector<std::string> keys, vals;
  for (auto &read: txn.getReadSet()) {
    keys.push_back(read.first);
  }
  if (keys.size() > 0) {
    store.BatchGet(keys, reply);
    keys.clear();
  }

  if (ver_keys.size() > 0) {
    store.GetNVersions(ver_keys, reply);
  }

  for (auto &write : txn.getWriteSet()) {
    keys.push_back(write.first);
    vals.push_back(write.second);
  }
  if (keys.size() > 0) {
    store.put(keys, vals, Timestamp(timestamp), reply);
  }

  prepared.erase(id);
}

int
OCCStore::GetProof(const std::map<uint64_t, std::vector<std::string>> &keys,
                   strongstore::proto::Reply* reply)
{
    // Get latest from store
    if (store.GetProof(keys, reply)) {
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

int
OCCStore::GetProof(const uint64_t seq,
                  strongstore::proto::Reply* reply)
{
    store.GetDigest(reply);
    if (store.GetProof(seq, reply)) {
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

void
OCCStore::Commit(uint64_t id, uint64_t timestamp,
                 strongstore::proto::Reply* reply)
{
  Transaction txn = prepared[id];

  store.GetDigest(reply);

  std::vector<std::string> keys, vals;
  for (auto &write : txn.getWriteSet()) {
    keys.push_back(write.first);
    vals.push_back(write.second);
  }
  store.put(keys, vals, Timestamp(timestamp), nullptr);

  prepared.erase(id);
}

set<string>
OCCStore::getPreparedWrites()
{
  // gather up the set of all writes that we are currently prepared for
  set<string> writes;
  for (auto &t : prepared) {
    for (auto &write : t.second.getWriteSet()) {
      writes.insert(write.first);
    }
  }
  return writes;
}

set<string>
OCCStore::getPreparedReadWrites()
{
  // gather up the set of all writes that we are currently prepared for
  set<string> readwrites;
  for (auto &t : prepared) {
    for (auto &write : t.second.getWriteSet()) {
      readwrites.insert(write.first);
    }
    for (auto &read : t.second.getReadSet()) {
      readwrites.insert(read.first);
    }
  }
  return readwrites;
}

} // namespace strongstore
