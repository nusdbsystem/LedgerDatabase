#ifndef QLDB_STORE_H_
#define QLDB_STORE_H_

#include <string>
#include "rocksdb/db.h"
#include "ledger/common/chunk.h"
#include "ledger/common/slice.h"

namespace ledgebase {

namespace qldb {

class QLDBStore {

 public:
  QLDBStore() { total_ = 0; }
  ~QLDBStore() = default;

  inline bool Open(const std::string& db_path) {
    db_opts_.error_if_exists = false;
    db_opts_.create_if_missing = true;

    rocksdb::DB::Open(db_opts_, db_path, &db_).ok();
  }

  std::string GetString(const std::string& key);
  const Chunk* Get(const std::string& key);
  bool Put(const std::string& key, const Slice& value);
  bool Put(const std::string& key, const Chunk& value);
  bool Exists(const std::string& key);

  inline void CreateCache(const std::string& id, Chunk&& chunk) {
    cache_.emplace(id, std::move(chunk));
  }

  size_t size() { return total_; }

 private:
  inline Chunk ToChunk(const rocksdb::Slice& x) const {
    const auto data_size = x.size();
    std::unique_ptr<byte_t[]> buf(new byte_t[data_size]);
    std::memcpy(buf.get(), x.data(), data_size);
    return Chunk(std::move(buf));
  }

  std::unordered_map<std::string, Chunk> cache_;
  rocksdb::DB* db_;
  rocksdb::Options db_opts_;
  size_t total_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // QLDB_STORE_H_
