#ifndef QLDB_QL_BTREE_H_
#define QLDB_QL_BTREE_H_

#include <vector>
#include <map>

#include "ledger/common/chunk.h"
#include "ledger/common/db.h"
#include "ledger/common/slice.h"
#include "ledger/qldb/ql_btree_node.h"

namespace ledgebase {

namespace qldb {

class QLBTree {
 public:
  static Chunk kEmptyMap;
  QLBTree(DB* db, const std::string& prefix) noexcept;
  QLBTree& operator=(QLBTree&&) = default;
  ~QLBTree() = default;

  Slice Get(const Slice& key) const;

  std::map<std::string, std::string> Range(const Slice& start,
      const Slice& end) const;
  
  bool Set(const Slice& key, const Slice& val);
  
  bool Set(const std::vector<Slice>& keys, const std::vector<Slice>& vals);
  
  inline uint64_t numElements() const { return root_node_->numElements(); }

 private:
  Slice TryGet(const Chunk* node, const Slice& key) const;
  
  void TryRange(const Chunk* node, const Slice& start, const Slice& end,
      std::map<std::string, std::string>& result) const;

  QLBTreeInsertResult Insert(const Chunk* node, const Slice& key,
      const Slice& value) const;

  Chunk FindChildNode(const Chunk* node, const Slice& target, int* index) const;

  void FindKey(const Chunk* node, const Slice& from,
      const Slice& to, std::map<std::string, std::string>& result) const;

  DB* db_;
  std::string prefix_;
  std::unique_ptr<const QLBTreeNode> root_node_;
  std::unique_ptr<QLBTreeDelta> delta_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // QLDB_QL_BTREE_H_
