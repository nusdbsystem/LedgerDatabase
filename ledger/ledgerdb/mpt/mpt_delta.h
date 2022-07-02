#ifndef MPT_DELTA_H_
#define MPT_DELTA_H_

#include <map>
#include "ledger/common/db.h"

#include "ledger/common/hash.h"
#include "ledger/common/chunk.h"

namespace ledgebase {

namespace ledgerdb {

class MPTDelta {
 public:
  MPTDelta(DB* db) : db_(db) {}
  ~MPTDelta() = default;

  void Commit(const Chunk* root);

  inline void CreateChunk(const Hash& hash, Chunk&& chunk) {
    if (dirty_.find(hash) == dirty_.end()) {
      dirty_.emplace(hash.Clone(), std::move(chunk));
    }
  }

  Chunk GetChunk(const Hash& hash) {
    std::map<Hash, Chunk>::iterator it = dirty_.find(hash);
    if (it == dirty_.end()) {
      return Chunk();
    }
    return Chunk(dirty_[hash].head());
  }

  inline bool empty() const { return dirty_.empty(); }

  inline size_t size() const { return dirty_.size(); }

  inline void Clear() { dirty_.clear(); }

  inline std::map<Hash, Chunk>& dirty() {return dirty_;}

 protected:
  DB* db_;
  std::map<Hash, Chunk> dirty_;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // MPT_DELTA_H_