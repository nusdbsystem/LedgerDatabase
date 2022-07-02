#ifndef LEDGERDB_TYPES_H
#define LEDGERDB_TYPES_H

#include <string>
#include "ledger/common/utils.h"

namespace ledgebase {

namespace ledgerdb {

struct CommitInfo {
  uint64_t commit_seq;
  std::string prev_digest;
  uint64_t tip_block;
  std::string mtroot;
  std::string mtrootkey;
  std::string mptroot;

  CommitInfo() {}
  CommitInfo(uint64_t cs, std::string pd, uint64_t tb, std::string mr,
      std::string mrk, std::string mpr)
      : commit_seq(cs), prev_digest(pd), tip_block(tb),
        mtroot(mr), mtrootkey(mrk), mptroot(mpr) {}
  CommitInfo(std::string str) {
    auto items = ledgebase::Utils::splitBy(str, '|');
    commit_seq = std::stoul(items[0]);
    prev_digest = items[1];
    tip_block = std::stoul(items[2]);
    mtroot = items[3];
    mtrootkey = items[4];
    mptroot = items[5];
  }

  std::string ToString() {
    return std::to_string(commit_seq) + "|" +
           prev_digest + "|" +
           std::to_string(tip_block) + "|" +
           mtroot + "|" +
           mtrootkey + "|" +
           mptroot;
  }
};

struct DigestInfo {
  uint64_t commit_seq;
  uint64_t tip_block;
  std::string digest;
  
  std::string ToString() {
    return std::to_string(commit_seq) + "|" +
        std::to_string(tip_block) + "|" + digest;
  }
  DigestInfo(uint64_t cs, uint64_t tb, std::string di)
      : commit_seq(cs), tip_block(tb), digest(di) {}
  DigestInfo(std::string str) {
    auto items = ledgebase::Utils::splitBy(str, '|');
    commit_seq = std::stoul(items[0]);
    tip_block = std::stoul(items[1]);
    digest = items[2];
  }
};

struct BlockData {
  std::vector<std::string> keys;
  std::vector<std::string> vals;
  std::string ToString() {
    std::string res = "";
    for (size_t i = 0; i < keys.size(); ++i) {
      res += "/" + keys[i] + "|" + vals[i];
    }
    res.substr(1);
    return res;
  }
  BlockData(const std::vector<std::string>& ks,
      const std::vector<std::string>& vs) : keys(ks), vals(vs) {}
  BlockData(std::string str) {
    auto kvs = ledgebase::Utils::splitBy(str, '/');
    for (auto& kv : kvs) {
      auto items = ledgebase::Utils::splitBy(str, '|');
      keys.emplace_back(items[0]);
      vals.emplace_back(items[1]);
    }
  }
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // LEDGERDB_TYPES_H