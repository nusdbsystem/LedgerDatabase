#include "ledger/ledgerdb/merkletree.h"
#include "ledger/common/slice.h"

namespace ledgebase {

namespace ledgerdb {

void MerkleTree::update(const uint64_t starting_block_seq,
                        const std::vector<std::string> &leaf_block_hashes,
                        const std::string& prev_commit_seq,
                        std::string* root_key,
                        std::string* root_hash) {
  // commit leaf hashes
  int level = 0;
  uint64_t level_starting_seq = starting_block_seq;
  bool prev_complete = starting_block_seq > 1 ?
      (starting_block_seq - 1) % 2 == 1 : true;
  bool complete = true;
  std::vector<Hash> level_hashes;
  for (size_t i = 0; i < leaf_block_hashes.size(); ++i) {
    ledger_->Put("mt0-" + std::to_string(starting_block_seq + i),
        leaf_block_hashes[i]);
    level_hashes.emplace_back(Hash::FromBase32(leaf_block_hashes[i]));
  }

  std::string commit_seq = prev_commit_seq.size() == 0 ?
      "0" : std::to_string(std::stoul(prev_commit_seq) + 1);
  while (level_hashes.size() > 1 || level_starting_seq > 0) {
    std::vector<Hash> parent_hashes;
    // load previous hash when needed
    if (level_starting_seq % 2 == 1) {
      std::string prev_key = "mt" + std::to_string(level) + "-" +
          std::to_string(level_starting_seq - 1);
      if (level > 0 && !prev_complete) {
          prev_key += "-" + prev_commit_seq;
      }
      std::string prev_hash;
      ledger_->Get(prev_key, &prev_hash);
      level_hashes.insert(level_hashes.begin(), Hash::FromBase32(prev_hash));
      --level_starting_seq;
      prev_complete = level_starting_seq > 1 ?
          (level_starting_seq - 1) % 2 == 1 : true;
    }
    for (size_t i = 0; i < level_hashes.size(); i = i + 2) {
      std::string parent_key = "mt" + std::to_string(level + 1) + "-" +
          std::to_string((level_starting_seq + i)/2);
      if (i + 1 < level_hashes.size()) {
        std::unique_ptr<ledgebase::byte_t[]> data(
            new ledgebase::byte_t[ledgebase::Hash::kByteLength * 2]);
        memcpy(data.get(), level_hashes[i].value(), Hash::kByteLength);
        memcpy(data.get() + Hash::kByteLength, level_hashes[i+1].value(),
            Hash::kByteLength);
        auto parent = Hash::ComputeFrom(data.get(), Hash::kByteLength*2);
        if (i + 1 == level_hashes.size() - 1 && !complete) {
          parent_key += "-" + commit_seq;
        }
        ledger_->Put(parent_key, parent.ToBase32());
        parent_hashes.emplace_back(parent.Clone());
      } else {
        auto parent = Hash::ComputeFrom(level_hashes[i].value(),
            Hash::kByteLength);
        parent_key += "-" + commit_seq;
        complete = false;
        ledger_->Put(parent_key, parent.ToBase32());
        parent_hashes.emplace_back(parent.Clone());
      }
    }
    level_hashes = std::move(parent_hashes);
    level_starting_seq /= 2;
    ++level;
  }
  *root_key = "mt" + std::to_string(level) + "-0" +
      (complete? "": ("-" + commit_seq));
  *root_hash = level_hashes[0].ToBase32();
}

Proof MerkleTree::getProof(const std::string& commit_seq,
                           const std::string& root_key,
                           const uint64_t tip,
                           const uint64_t seq) const {
  Proof proof;
  std::string digest, value;
  ledger_->Get(root_key, &digest);
  ledger_->Get("mt0-" + std::to_string(seq), &value);
  proof.digest = digest;
  proof.value = value;
  bool complete = (tip % 2 == 1);

  auto delim = root_key.find("-");
  int level = std::stoi(root_key.substr(2, delim - 2));
  //std::cout << "height " << level << std::endl;
  uint64_t ptr = seq;
  uint64_t last = tip;
  for (int i = 0; i < level; ++i) {
    std::string res;
    if (ptr % 2  == 0) {
      proof.pos.emplace_back(1);
      if (ptr == last) {
        res = "";
      } else {
        std::string sibling_key =
            "mt" + std::to_string(i) + "-" + std::to_string(ptr+1);
        if (ptr + 1 == last && i > 0 && !complete) {
          sibling_key += "-" + commit_seq;
        }
        ledger_->Get(sibling_key, &res);
      }
    } else {
      std::string sibling_key =
          "mt" + std::to_string(i) + "-" + std::to_string(ptr-1);
      ledger_->Get(sibling_key, &res);
      proof.pos.emplace_back(0);
    }
    proof.proof.emplace_back(res);
    ptr /= 2;
    last /= 2;
    complete = complete && (last % 2 == 1);
  }
  return proof;
}

bool Proof::Verify() const {
  auto calc_hash = Hash::FromBase32(value);  //Hash::ComputeFrom(value);
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
  if (calc_hash == Hash::FromBase32(digest)) return true;
  return false;
}

}  // namespace ledgerdb

}  // namespace ledgebase
