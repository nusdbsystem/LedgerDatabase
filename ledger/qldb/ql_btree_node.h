#ifndef QLDB_QL_BTREE_NODE_H_
#define QLDB_QL_BTREE_NODE_H_

#include <vector>

#include "ledger/qldb/ql_btree_delta.h"
#include "ledger/common/slice.h"

namespace ledgebase {

namespace qldb {

struct QLBTreeInsertResult {
  bool isSplit;
  std::string split_key;
  size_t level;
  std::vector<uint64_t> num_elems;
};

class QLBTreeNode {

 public:
  static std::unique_ptr<const QLBTreeNode> CreateFromChunk(const Chunk* chunk);

  explicit QLBTreeNode(const Chunk* chunk) : chunk_(chunk) {}
  ~QLBTreeNode() = default;  // NOT delete chunk!!

  inline const byte_t* head() const { return chunk_->head(); }

  inline const byte_t* data() const { return chunk_->data(); }

  inline const Chunk* chunk() const { return chunk_; }

  inline uint32_t numBytes() const { return chunk_->numBytes(); }

  inline uint64_t numElements() const {
      return *reinterpret_cast<const uint64_t*>(chunk_->data()); }

  inline uint32_t numEntries() const {
    return *reinterpret_cast<const uint32_t*>(chunk_->data() +
        sizeof(uint64_t) + sizeof(uint32_t));
  }

 protected:
  const Chunk* chunk_;
};

/**
 * Internal nodes encoding scheme
 * |-num_elements-|-level-|-num_entries-|-key_bytes-|-keys-|-num_elements-|...
 *        -num_elements-|
 * 
 * |------ 8 -----|---4---|----- 4 -----|---- 4 ----|-var--|------ 8 -----|...
 *        ------ 8 -----|
 */
class QLBTreeMeta : public QLBTreeNode {
 public:
  static Chunk Encode(size_t level, const std::vector<Slice>& keys,
      const std::vector<uint64_t>& num_elems);

  explicit QLBTreeMeta(const Chunk* chunk) : QLBTreeNode(chunk) {
      PrecomputeOffset(); }

  ~QLBTreeMeta() = default;  // NOT delete chunk!!

  size_t BinarySearch(const Slice& key, int start, int end) const;

  QLBTreeInsertResult UpdateChild(size_t index,
      const QLBTreeInsertResult& retval, QLBTreeDelta* bp_delta,
      const std::string& prefix) const;

  inline size_t GetLevel() const { return level_; }

  inline Slice GetKey(size_t index) const { return keys_[index]; }

  inline uint64_t GetChildElems(size_t index) const {
      return num_elems_[index]; }

 private:
  void PrecomputeOffset();

  size_t level_;
  std::vector<Slice> keys_;
  std::vector<uint64_t> num_elems_;
};

/**
 * Leaf encoding scheme
 * |-num_elements-|-level-|-num_entries-|-n_bytes-|-key_bytes-|-key-|-val-|...
 *          -next_bytes-|-next-|
 * 
 * |------ 8 -----|---4---|----- 4 -----|--- 4 ---|---- 4 ----|-var-|-var-|...
 *          ----- 4 ----|--var-|
 */
class QLBTreeMap : public QLBTreeNode {
 public:
  static Chunk Encode(size_t level, const std::vector<Slice>& keys,
      const std::vector<Slice>& vals, const Slice& next);

  static Chunk EmptyMap();

  explicit QLBTreeMap(const Chunk* chunk) : QLBTreeNode(chunk) {
      PrecomputeOffset(); }

  ~QLBTreeMap() = default;  // NOT delete chunk!!

  Slice BinarySearch(const Slice& key, int start, int end, size_t* index) const;

  inline size_t GetLevel() const { return level_; }

  inline Slice GetKey(size_t index) const { return keys_[index]; }

  inline Slice GetVal(size_t index) const { return vals_[index]; }

  inline Slice GetNext() const { return next_; }

 private:
  void PrecomputeOffset();

  size_t level_;
  std::vector<Slice> keys_;
  std::vector<Slice> vals_;
  Slice next_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // QLDB_QL_BTREE_NODE_H_