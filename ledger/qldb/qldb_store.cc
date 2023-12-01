#include "ledger/qldb/qldb_store.h"

#include <utility>

namespace ledgebase {

namespace qldb {

std::string QLDBStore::GetString(const std::string& key) {
  rocksdb::PinnableSlice value;
  if (db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(),
               rocksdb::Slice(key), &value).ok()) {
    std::string result;
    result.resize(value.size());
    memcpy(&result[0], value.data(), value.size());
    return result;
  } else {
    return "";
  }
}

const Chunk* QLDBStore::Get(const std::string& key) {
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return &it->second;
  }
  rocksdb::PinnableSlice value;
  if (db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(),
               rocksdb::Slice(key), &value).ok()) {
    cache_.emplace(key, ToChunk(value));
    return &cache_[key];
  } else {
    cache_.emplace(key, Chunk());
    return &cache_[key];
  }
}

bool QLDBStore::Put(const std::string& key, const Slice& value) {
  auto res = GetString(key);
  total_ = total_ + (res.size() > 0? 0:key.size()) + value.len() - res.size();
  return db_->Put(rocksdb::WriteOptions(), rocksdb::Slice(key),
                  rocksdb::Slice(reinterpret_cast<const char*>(value.data()),
                                 value.len())).ok();
}

bool QLDBStore::Put(const std::string& key, const Chunk& chunk) {
  auto res = Get(key);
  total_ = total_ + res->empty()? (key.size() + chunk.numBytes()): (chunk.numBytes() - res->numBytes());
  std::cout << total_ << std::endl;
  cache_.erase(key);
  return db_->Put(rocksdb::WriteOptions(), rocksdb::Slice(key),
                  rocksdb::Slice(reinterpret_cast<const char*>(chunk.head()),
                                chunk.numBytes())).ok();
}

bool QLDBStore::Exists(const std::string& key) {
  rocksdb::PinnableSlice value;
  return db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(),
                  rocksdb::Slice(key), &value).ok();
}

}  // namespace qldb

}  // namespace ledgebase
