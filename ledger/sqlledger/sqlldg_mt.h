#ifndef SQLLEDGER_MERKLETREE_H
#define SQLLEDGER_MERKLETREE_H

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

namespace sqlledger {

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

  void Build(const std::string& mtid, const std::vector<std::string> &leaves,
      std::string* level, std::string* root_hash);
  Proof GetProof(const std::string& mtid, const int& level,
      const uint64_t& seq) const;

 private:
  DB *ledger_;
};

}  // namespace sqlledger

}  // namespace ledgebase

#endif  // SQLLEDGER_MERKLETREE_H
