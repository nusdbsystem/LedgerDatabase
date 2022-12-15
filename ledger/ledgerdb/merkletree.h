#ifndef LEDGERDB_MERKLETREE_H
#define LEDGERDB_MERKLETREE_H

#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <openssl/sha.h>
#include <chrono>
#include "ledger/common/hash.h"
#include "ledger/common/db.h"

namespace ledgebase {

namespace ledgerdb {

struct Proof {
  std::string digest;
  std::string value;
  std::vector<std::string> proof;
  std::vector<size_t> pos;
  bool Verify() const;
};

class MerkleTree {
 public:
  static std::string sha256(std::string data) {
    auto digest = Hash::ComputeFrom(data);
    return digest.ToBase32();
  }

  MerkleTree(DB *db) : ledger_(db) { }
  ~MerkleTree() = default;

  void update(const uint64_t blk_seq, const std::vector<std::string> &blk_hashes,
      const std::string& prev_commit_seq, std::string* root_key,
      std::string* root_hash);
  Proof getProof(const std::string& commit_seq, const std::string& root_key,
      const uint64_t tip, const uint64_t target_block_seq) const;

 private:
  DB *ledger_;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif
