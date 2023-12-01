#ifndef LEDGERDB_LEDGERDB_H
#define LEDGERDB_LEDGERDB_H

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unistd.h>

#include "tbb/concurrent_queue.h"

#include "ledger/common/db.h"
#include "ledger/ledgerdb/merkletree.h"
#include "ledger/ledgerdb/mpt/trie.h"
#include "ledger/ledgerdb/mpt/mpt_config.h"
#include "ledger/ledgerdb/skiplist/skiplist.h"

namespace ledgebase {

namespace ledgerdb {

class KeyLockMgr {
 public:
  KeyLockMgr() = default;

  ~KeyLockMgr() {
    for (auto& v : key_locks)
      delete v.second;
  }

  void LockKey(std::string key) {
    std::mutex *key_mu;

    mu.lock();
    auto it_lock = key_locks.find(key);
    if (it_lock == key_locks.end()) {
      auto km = new std::mutex();
      key_locks.insert(std::pair<std::string, std::mutex *>(key, km));
      key_lock_ref.insert(std::pair<std::string, int>(key, 1));
      key_mu = km;
    } else {
      key_mu = it_lock->second;
      key_lock_ref.find(key)->second++;
    }
    mu.unlock();

    key_mu->lock();
  }

  void UnlockKey(std::string key) {
    mu.lock();
    auto it_lock = key_locks.find(key);
    if (it_lock == key_locks.end()) {
      mu.unlock();
      return;
    }
    it_lock->second->unlock();
    if (--key_lock_ref.find(key)->second == 0) {
      delete it_lock->second;
      key_locks.erase(key);
      key_lock_ref.erase(key);
    }
    mu.unlock();
  }

 private:
  std::mutex mu;
  std::unordered_map<std::string, std::mutex *> key_locks;
  std::unordered_map<std::string, int> key_lock_ref;
};

struct Tree_Block {
  size_t blk_seq;
  std::vector<std::string> mpt_ks;
  std::string mpt_ts;
};

struct Auditor {
  std::string digest;
  uint64_t commit_seq;
  uint64_t first_block_seq;
  std::vector<std::string> commits;
  std::vector<std::string> blocks;
  std::vector<MPTProof> mptproofs;

  bool Audit(DB* db);
};

class LedgerDB {
 public:
  LedgerDB(int timeout,
           std::string dbpath = "/tmp/testdb",
           std::string ledgerPath = "/tmp/testledger");

  ~LedgerDB();

  void buildTree(int timeout);

  uint64_t Set(const std::vector<std::string> &keys,
           const std::vector<std::string> &values,
           const uint64_t &timestamp);

  int binarySearch(std::vector<std::string> &vec, int l, int r, std::string key);

  bool GetValues(const std::vector<std::string> &keys,
                 std::vector<std::pair<uint64_t, std::pair<size_t, std::string>>> &values);

  bool GetRange(const std::string &start, const std::string &end,
                std::map<std::string, std::pair<uint64_t, std::pair<size_t, std::string>>> &values);
  
  bool GetVersions(const std::vector<std::string> &keys,
                   std::vector<std::vector<std::pair<uint64_t, std::pair<size_t, std::string>>>> &values,
                   size_t nversions);

  bool GetProofs(const std::vector<std::string> &keys,
                 const std::vector<size_t> key_blk_seqs,
                 std::vector<Proof> &mt_proofs,
                 std::vector<MPTProof> &mpt_proofs,
                 std::string *root_digest,
                 size_t *blk_seq,
                 std::string *mpt_hash);

  Auditor GetAudit(const uint64_t seq);
  
  bool GetRootDigest(uint64_t *blk_seq,
                     std::string *root_digest);

  inline size_t size() { return db_.size(); }

 private:
  std::string splitAndFind(const std::string &str, char delim, const::std::string &target);

  DB db_;
  //DB ledger_;
  std::atomic<bool> stop_;
  uint64_t next_block_seq_;
  uint64_t commit_seq_;
  std::unique_ptr<std::thread> buildThread_;
  tbb::concurrent_queue<Tree_Block> tree_queue_;
  std::unique_ptr<MerkleTree> mt_;
  std::unique_ptr<SkipList> sl_;
  std::map<std::string, long> skiplist_head_;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif
