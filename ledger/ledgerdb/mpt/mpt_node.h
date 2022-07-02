#ifndef MPT_NODE_H_
#define MPT_NODE_H_

#include <memory>
#include <vector>

#include "ledger/common/chunk.h"
#include "ledger/common/hash.h"

namespace ledgebase {

namespace ledgerdb {

static constexpr size_t kHashSize = Chunk::kMetaLength + Hash::kByteLength;

class MPTNode {
 public:
  static std::unique_ptr<const MPTNode> CreateFromChunk(const Chunk* chunk);

  explicit MPTNode(const Chunk* chunk) : chunk_(chunk) {}
  ~MPTNode() = default;

  inline uint32_t numBytes() const { return chunk_->numBytes(); }

  inline const unsigned char* data() const { return chunk_->data(); }

  inline const unsigned char* head() const {return chunk_->head(); }

  inline ChunkType getType() const { return chunk_->type(); }

  inline const Hash hash() const { return chunk_->hash(); }

  inline const Chunk* chunk() const { return chunk_; }

  uint64_t numElements() const;

 protected:
  const Chunk* chunk_;
};

class MPTFullNode : public MPTNode {
/**
 * Encoding scheme of Full Node
 * |-- num elements --|--- Hash Node ---|--- ... ---|-- Value Node --|
 * |------- 8 --------| ------ 25 ------|--- ... ---|--- var size ---|
 */
 public:
  static Chunk Encode(const std::vector<Chunk>& nodes);

  explicit MPTFullNode(const Chunk* chunk) : MPTNode(chunk) {
      PrecomputeOffset(); }
  MPTFullNode(MPTFullNode&&) = default;
  MPTFullNode& operator=(MPTFullNode&&) = default;
  ~MPTFullNode() = default;

  Chunk setChildAtIndex(size_t index, const Chunk* child,
      const Chunk* o_child) const;

  inline Chunk getChildAtIndex(size_t index) const {
      return Chunk(children_[index].head()); }

 private:
  void PrecomputeOffset();

  std::vector<Chunk> children_;
};

class MPTShortNode : public MPTNode {
/**
 * Encoding scheme of Short Node
 * |-- num elements --|---- key len ----|----- key -----|----- child ----|
 * |----------------- 8 -------------- 12 -- var size --|--- var size ---|
 */
 public:
  static Chunk Encode(const std::string& key, const Chunk* node);

  explicit MPTShortNode(const Chunk* chunk) : MPTNode(chunk) {
      PrecomputeOffset(); }
  ~MPTShortNode() = default;
  
  inline std::string getKey() const { return key_; }

  inline Chunk childNode() const { return Chunk(val_.head()); }

 private:
  void PrecomputeOffset();

  std::string key_;
  Chunk val_;
};

class MPTHashNode : public MPTNode {
/**
 * Encoding scheme of Hash Node
 * |----- Hash ----|
 * |--------------20
 */
 public:
  static Chunk Encode(const Chunk* child);

  explicit MPTHashNode(const Chunk* chunk) : MPTNode(chunk) {
      PrecomputeOffset(); }
  ~MPTHashNode() = default;

  inline const Hash childHash() const { return hash_; }

 private:
  void PrecomputeOffset();

  Hash hash_;
};

class MPTValueNode : public MPTNode {
/**
 * Encoding scheme of Value Node
 * |----- Value ----|
 * |--- var size ---|
 */
 public:
  static Chunk Encode(const std::string& value);

  explicit MPTValueNode(const Chunk* chunk) : MPTNode(chunk) {
      PrecomputeOffset(); }
  ~MPTValueNode() = default;  // NOT delete chunk!!

  inline std::string getVal() const { return val_; }

 private:
  void PrecomputeOffset();

  std::string val_;
};

class MPTNilNode : public MPTNode {
 public:
  static Chunk Encode();

  explicit MPTNilNode(const Chunk* chunk) : MPTNode(chunk) {}
  ~MPTNilNode() = default;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // MPT_NODE_H_