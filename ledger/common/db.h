#ifndef DB_H_
#define DB_H_

#include <thread>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/table.h"
#include "tbb/concurrent_hash_map.h"

#include "ledger/common/chunk.h"
#include "ledger/common/hash.h"

namespace ledgebase {

static const size_t kCacheSizeBytes(128 << 20);
static const size_t kWriteBufferSize(256 << 20);
static const uint64_t kMemtableMemoryBudget(1 << 30);

class DB {
 public:
  DB() { total_ = 0; };
  ~DB() = default;

  inline bool Open(const std::string& db_path) {
    rocksdb::Options options_db;
    options_db.error_if_exists = false;
    options_db.create_if_missing = true;
    options_db.IncreaseParallelism(std::thread::hardware_concurrency());
    options_db.OptimizeLevelStyleCompaction(kMemtableMemoryBudget);
    options_db.write_buffer_size = kWriteBufferSize;

    rocksdb::BlockBasedTableOptions db_blk_tab_opts;
    db_blk_tab_opts.block_cache = rocksdb::NewClockCache(kCacheSizeBytes);
    db_blk_tab_opts.filter_policy.reset(
        rocksdb::NewBloomFilterPolicy(10, true));
    options_db.table_factory.reset(
        rocksdb::NewBlockBasedTableFactory(db_blk_tab_opts));
    return rocksdb::DB::Open(options_db, db_path, &db_).ok();
  }

  inline void CreateCache(const std::string& id, Chunk&& chunk) {
    tbb::concurrent_hash_map<std::string, Chunk>::accessor a;
    m_cache_.insert(a, std::make_pair(id, std::move(chunk)));
  }

  inline bool Get(const std::string& key, std::string* value) const {
    return db_->Get(rocksdb::ReadOptions(), key, value).ok();
  }

  inline Chunk* Get(const std::string& key) {
    tbb::concurrent_hash_map<std::string, Chunk>::accessor a;
    if (m_cache_.find(a, key)) return &(a->second);
    std::string value;
    if (db_->Get(rocksdb::ReadOptions(), key, &value).ok()) {
      m_cache_.insert(a, std::make_pair(key, ToChunk(value)));
    } else {
      m_cache_.insert(a, std::make_pair(key, Chunk()));
    }
    return &(a->second);
  }

  inline Chunk* Get(const Hash& key) {
    tbb::concurrent_hash_map<std::string, Chunk>::accessor a;
    if (cache_.find(a, key.ToBase32())) return &(a->second);
    cache_.insert(a, std::make_pair(key.ToBase32(), GetChunk(key)));
    return &(a->second);
  }

  inline bool Scan(const std::string& start, const std::string& end,
      std::map<std::string, std::string>& res) {
    auto iter = db_->NewIterator(rocksdb::ReadOptions());
    for (iter->Seek(start); iter->Valid() && iter->key().ToString() < end;
        iter->Next()) {
      res.emplace(iter->key().ToString(), iter->value().ToString());
    }
    return true;
  }

  inline Chunk GetChunk(const Hash& hash) const {
    rocksdb::PinnableSlice value;
    if (db_->Get(rocksdb::ReadOptions(),
        db_->DefaultColumnFamily(), ToRocksSlice(hash), &value).ok()) {
      auto chunk = ToChunk(value);
      return chunk;
    } else {
      return Chunk();
    }
  }

  inline bool Put(const Hash& key, const Chunk& chunk) {
    //rocksdb::PinnableSlice pin_val;
    const auto key_slice = ToRocksSlice(key);
    //if (!db_->Get(rocksdb::ReadOptions(), db_->DefaultColumnFamily(), key_slice, &pin_val).ok()) {
    //  total_ += (long) (Hash::kByteLength + chunk.numBytes());
    //}
    return db_->Put(rocksdb::WriteOptions(), key_slice, ToRocksSlice(chunk)).ok();
  }

  inline bool Put(const std::string& key, const Chunk& value) {
    //auto res = Get(key);
    //long temp = total_;
    //total_ = total_ + (res->empty()? (long) (key.size() + value.numBytes()):
    //    ((long) value.numBytes() - (long) res->numBytes()));
    //if (temp > total_) {std::cout << "[Error]" << std::endl;}
    //std::cout << total_ << std::endl;
    m_cache_.erase(key);
    return db_->Put(rocksdb::WriteOptions(), rocksdb::Slice(key),
        rocksdb::Slice(reinterpret_cast<const char*>(value.head()),
        value.numBytes())).ok();
  }

  inline bool Put(const std::string& key, const std::string& val) {
    //std::string res;
    //Get(key, &res);
    //long temp = total_;
    //total_ = total_ + (res.size() > 0? ((long) val.size() - (long) res.size())
    //    : (long)(key.size() + val.size()));
    //if (temp > total_) {std::cout << "[Error]" << std::endl;}
    //std::cout << total_ << std::endl;
    return db_->Put(rocksdb::WriteOptions(), key, val).ok();
  }

  inline bool Put(rocksdb::WriteBatch* batch) {
    return db_->Write(rocksdb::WriteOptions(), batch).ok();
  }

  inline rocksdb::Iterator* NewIterater() {
    return db_->NewIterator(rocksdb::ReadOptions());
  }

  inline long size() { return total_; }

 private:
  inline Chunk ToChunk(const rocksdb::Slice& x) const {
    const auto data_size = x.size();
    std::unique_ptr<unsigned char[]> buf(new unsigned char[data_size]);
    std::memcpy(buf.get(), x.data(), data_size);
    return Chunk(std::move(buf));
  }

  inline rocksdb::Slice ToRocksSlice(const Hash& x) const {
    return rocksdb::Slice(reinterpret_cast<const char*>(x.value()),
                          Hash::kByteLength);
  }

  inline rocksdb::Slice ToRocksSlice(const Chunk& x) const {
    return rocksdb::Slice(reinterpret_cast<const char*>(x.head()),
                          x.numBytes());
  }

  rocksdb::DB* db_;
  long total_;
  tbb::concurrent_hash_map<std::string, Chunk> cache_;
  tbb::concurrent_hash_map<std::string, Chunk> m_cache_;
};

}  // namespace ledgebase

#endif  // DB_H_
