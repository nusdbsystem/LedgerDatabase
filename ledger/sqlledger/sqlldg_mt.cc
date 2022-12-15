#include "ledger/sqlledger/sqlldg_mt.h"
#include "ledger/common/slice.h"

namespace ledgebase {

namespace sqlledger {

void MerkleTree::Build(const std::string& mtid,
    const std::vector<std::string> &leaves, std::string* toplevel,
    std::string* root_hash) {
  if (leaves.size() == 0) return;
  // commit leaf hashes
  int level = 0;
  std::vector<Hash> level_hashes;
  for (size_t i = 0; i < leaves.size(); ++i) {
    auto leaf_hash = Hash::ComputeFrom(leaves[i]);
    level_hashes.emplace_back(leaf_hash.Clone());
    ledger_->Put(mtid + "|0|" + std::to_string(i), leaf_hash.ToBase32());
  }

  while (level_hashes.size() > 1) {
    std::vector<Hash> parent_hashes;
    for (size_t i = 0; i < level_hashes.size(); i = i + 2) {
      std::string parent_key = mtid + "|" + std::to_string(level + 1) + "|" +
          std::to_string(i/2);
      if (i + 1 < level_hashes.size()) {
        std::unique_ptr<ledgebase::byte_t[]> data(
            new ledgebase::byte_t[ledgebase::Hash::kByteLength * 2]);
        memcpy(data.get(), level_hashes[i].value(), Hash::kByteLength);
        memcpy(data.get() + Hash::kByteLength, level_hashes[i+1].value(),
            Hash::kByteLength);
        auto parent = Hash::ComputeFrom(data.get(), Hash::kByteLength*2);
        ledger_->Put(parent_key, parent.ToBase32());
        parent_hashes.emplace_back(parent.Clone());
      } else {
        auto parent = Hash::ComputeFrom(level_hashes[i].value(),
            Hash::kByteLength);
        ledger_->Put(parent_key, parent.ToBase32());
        parent_hashes.emplace_back(parent.Clone());
      }
    }
    level_hashes = std::move(parent_hashes);
    ++level;
  }
  *toplevel = std::to_string(level);
  *root_hash = level_hashes[0].ToBase32();
}

Proof MerkleTree::GetProof(const std::string& mtid,
                           const int& level,
                           const uint64_t& seq) const {
  Proof proof;
  uint64_t ptr = seq;
  for (int i = 0; i < level; ++i) {
    std::string res;
    if (ptr % 2  == 0) {
      ledger_->Get(mtid + "|" + std::to_string(i) + "|" + std::to_string(ptr+1),
          &res);
      proof.pos.emplace_back(1);
    } else {
      ledger_->Get(mtid + "|" + std::to_string(i) + "|" + std::to_string(ptr-1),
          &res);
      proof.pos.emplace_back(0);
    }
    proof.proof.emplace_back(res);
    ptr /= 2;
  }
  std::string res;
  ledger_->Get(mtid + "|" + std::to_string(level) + "|0", &res);
  proof.digest = res;
  return proof;
}

bool Proof::Verify() const {
  auto calc_hash = Hash::ComputeFrom(value);
  for (size_t i = 0; i < proof.size(); ++i) {
    auto proof_hash = Hash::FromBase32(proof[i]);
    if (pos[i] == 0) {
      std::unique_ptr<ledgebase::byte_t[]> data(
          new ledgebase::byte_t[ledgebase::Hash::kByteLength * 2]);
      memcpy(data.get(), proof_hash.value(), Hash::kByteLength);
      memcpy(data.get() + Hash::kByteLength, calc_hash.value(),
          Hash::kByteLength);
      calc_hash = Hash::ComputeFrom(data.get(), Hash::kByteLength*2);
    } else if (proof[i].size() > 0) {
      std::unique_ptr<ledgebase::byte_t[]> data(
          new ledgebase::byte_t[ledgebase::Hash::kByteLength * 2]);
      memcpy(data.get(), calc_hash.value(), Hash::kByteLength);
      memcpy(data.get() + Hash::kByteLength, proof_hash.value(),
          Hash::kByteLength);
      calc_hash = Hash::ComputeFrom(data.get(), Hash::kByteLength*2);
    } else {
      calc_hash = Hash::ComputeFrom(calc_hash.value(), Hash::kByteLength);
    }
  }
  //std::cout << "tree height " << proof.size() << std::endl;
  if (calc_hash == Hash::FromBase32(digest)) return true;
  return false;
}

}  // namespace sqlledger

}  // namespace ledgebase
