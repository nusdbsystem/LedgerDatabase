#ifndef QLDB_BTREE_DELTA_H_
#define QLDB_BTREE_DELTA_H_

#include <map>

#include "ledger/common/db.h"
#include "ledger/common/chunk.h"

namespace ledgebase {

namespace qldb {

class QLBTreeDelta {
 public:
  QLBTreeDelta(DB* db) : db_(db) {}
  ~QLBTreeDelta() = default;

  void Commit(const std::string& id, bool isroot, const std::string& prefix);

  inline void CreateChunk(const std::string& id, Chunk&& chunk) {
    std::map<std::string, Chunk>::iterator it = dirty_.find(id);
    if (it == dirty_.end()) {
      dirty_.emplace(id, std::move(chunk));
    } else {
      it->second = std::move(chunk);
    }
  }

  Chunk GetChunk(const std::string& id) {
    auto it = dirty_.find(id);
    if (it == dirty_.end()) {
      return Chunk();
    }
    return Chunk(it->second.head());
  }

  inline bool empty() const { return dirty_.empty(); }

  inline size_t size() const { return dirty_.size(); }

  inline void Clear() { dirty_.clear(); }

  inline std::map<std::string, Chunk>& dirty() {return dirty_;}

 protected:
  DB* db_;
  std::map<std::string, Chunk> dirty_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // QLDB_BTREE_DELTA_H_