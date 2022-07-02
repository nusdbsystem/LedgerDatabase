#ifndef QLDB_QLNODE_H_
#define QLDB_QLNODE_H_

#include <vector>
#include "ledger/common/chunk.h"
#include "ledger/common/slice.h"

namespace ledgebase {

namespace qldb {

struct BlockAddress {
  Slice ledger_name;
  uint64_t seq_no;
};

struct Data {
  Slice key;
  Slice val;
};

struct MetaData {
  size_t doc_seq;
  size_t version;
  uint64_t time;
};

struct Statement {
  byte_t type;
  uint64_t time;
};

class QLNode {
 public:
  explicit QLNode(const Chunk* chunk) : chunk_(chunk) {}
  ~QLNode() = default;  // NOT delete chunk!!

  inline uint32_t numBytes() const { return chunk_->numBytes(); }

  inline const byte_t* data() const { return chunk_->data(); }

  inline const byte_t* head() const {return chunk_->head(); }

  inline const Hash hash() const {return chunk_->hash(); }

  inline ChunkType getType() const { return chunk_->type(); }

 protected:
  const Chunk* chunk_;
};

/**
 * | ldg_bytes | ledger_nm | seq | entry_sz | key_sz | key
 * | --- 4 --- | -- var -- | -8- | -- 4 --  |  - 4 - | var
 *       | value | doc_seq | version | time |
 *       | -var- | -- 4 -- | -- 4 -- | - 8 -|
 */
class Document : public QLNode {
 public:
  static const size_t fixed_length = 36;
  static Chunk Encode(const Slice& key,
                      const Slice& val,
                      const BlockAddress& addr,
                      const MetaData& meta);
  
  explicit Document(const Chunk* chunk) : QLNode(chunk) {
      PrecomputeOffset(); }
  ~Document() = default;  // NOT delete chunk!!

  inline BlockAddress getAddr() const { return addr_; }

  inline Data getData() const { return data_; }

  inline MetaData getMetaData() const { return metadata_; }

 private:
  void PrecomputeOffset();

  BlockAddress addr_;
  Data data_;
  MetaData metadata_;
};

class QLBlock : public QLNode {
/**
 * | hash | prev_hash | block_time | lg_sz | lg_nm | seq_no | stmt_type
 * | -20- | -- 20 --  | --- 8 ---  | - 4 - | -var- | - 8 -  | --- 1 ---
 *     | stmt_time | document | document |
 *     | --- 8 --- | -- var --| -- var --|
 */
 public:
  static const size_t fixed_length = 69;
  static Chunk Encode(const BlockAddress& addr,
                      const Statement& statement,
                      const uint64_t block_time,
                      const Hash& block_hash,
                      const Hash& previous_hash,
                      const std::vector<Chunk>& documents);

  static void UpdateHash(Chunk* block, const Hash& block_hash);

  explicit QLBlock(const Chunk* chunk) : QLNode(chunk) {
      PrecomputeOffset(); }
  ~QLBlock() = default;  // NOT delete chunk!!

  inline BlockAddress getAddr() const { return addr_; }

  inline Hash getBlockHash() const { return block_hash_; }

  inline size_t getDocumentSize() const { return documents_.size(); }

  inline Chunk getDocumentBySeq(size_t seq) const {
      return Chunk(documents_[seq].head()); }

 private:
  void PrecomputeOffset();

  BlockAddress addr_;
  std::vector<Chunk> documents_;
  Statement statement_;
  uint64_t block_time_;
  Hash block_hash_;
  Hash previous_hash_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // QLDB_QLNODE_H_
