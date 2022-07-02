#ifndef MPT_TRIE_H_
#define MPT_TRIE_H_

#include <vector>
#include <map>

#include "ledger/ledgerdb/mpt/mpt_delta.h"
#include "ledger/ledgerdb/mpt/mpt_node.h"
#include "ledger/common/db.h"

namespace ledgebase {

namespace ledgerdb {

class MPTProof {
 public:
  MPTProof():size_(0) {}

  bool VerifyProof(const Hash& digest, const std::string& key) const;

  inline void AppendProof(const unsigned char* value, size_t pos) {
    Chunk chunk(value);
    std::string data(reinterpret_cast<const char*>(chunk.head()),
        chunk.numBytes());
    map_chunks_.emplace_back(data);
    map_pos_.emplace_back(pos);

    size_ += *reinterpret_cast<const uint32_t*>(value + Chunk::kNumBytesOffset);
  }

  inline std::string GetMapChunk(size_t idx) const {
      return map_chunks_[idx]; }
  
  inline size_t MapSize() const { return map_chunks_.size(); }

  inline size_t GetMapPos(size_t idx) const { return map_pos_[idx]; }

  inline void SetValue(const std::string& val) { value_ = val; }
  inline std::string GetValue() const { return value_; }

  inline size_t size() { return size_; }

 private:
  size_t size_;
  std::string value_;
  std::vector<std::string> map_chunks_;
  std::vector<size_t> map_pos_;
};

class Trie {
 public:
  static Chunk kNilChunk;

  Trie(Trie&&) = default;
  Trie(DB* db, const Hash& root_hash) noexcept;
  Trie(DB* db, const std::vector<std::string>& keys,
      const std::vector<std::string>& vals) noexcept;
  Trie& operator=(Trie&&) = default;
  ~Trie() = default;

  std::string Get(const std::string& key) const;

  MPTProof GetProof(const std::string& key) const;
  
  Hash Set(const std::string& key, const std::string& val) const;
  
  Hash Set(const std::vector<std::string>& keys,
      const std::vector<std::string>& vals) const;
  
  std::map<std::string, std::string> Diff(const Hash& rhs) const;

  Hash Remove(const std::string& key) const;
  
  inline uint64_t numElements() const { return root_node_->numElements(); }

  inline bool empty() const { return root_node_.get() == nullptr; }

  inline Hash hash() {
    return root_node_->hash();
  }

 private:
  bool SetNodeForHash(const Hash& root_hash);
  
  std::string TryGet(const Chunk* node, const std::string& key,
      const size_t pos) const;

  Chunk Insert(const Chunk* node, const std::string& key,
      const Chunk* value) const;

  Chunk TryRemove(const Chunk* node, std::string& prefix,
      std::string& key) const;

  Chunk GetHashNodeChild(const Chunk* hash_node) const;

  void Compare(const Chunk* lhs, const Chunk* rhs,
        size_t lkey_pos, size_t rkey_pos, std::string& key,
        std::map<std::string, std::string>& result) const;

  void GetAll(const Chunk* root, std::string& key,
        std::map<std::string, std::string>& result) const;

  void TryGetProof(const Chunk* node, const std::string& key,
        const size_t pos, MPTProof* proof) const;

  DB* db_;
  std::unique_ptr<const MPTNode> root_node_;
  std::unique_ptr<MPTDelta> mpt_delta_;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // MPT_TRIE_H_
