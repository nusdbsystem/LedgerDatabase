#ifndef LOGGER_H_
#define LOGGER_H_

#include <memory>
#include <vector>
#include <sys/time.h>                                                              
#include <sys/types.h>                                                             
#include <sys/stat.h>                                                              
#include <fcntl.h>                                                                 
#include <unistd.h>
#include <mutex>

#include "ledger/common/slice.h"
#include "ledger/common/chunk.h"

namespace ledgebase {

/*
 * | numEntries | keySize | key | valsize | val | signature | time | txnid |
 * |     4      |    4    | var |    4    | var |     20    |   8  |   8   |
 */
class TxnEntry {
 public:
  static std::vector<TxnEntry> ParseTxnEntries(const std::string& entries);
  static Chunk Encode(uint64_t time, const std::vector<std::string>& keys,
      const std::vector<std::string>& vals, const Hash& signature, 
      uint64_t txnid);

  TxnEntry(const Chunk* chunk) : chunk_(chunk) {Parse();}
  TxnEntry() : chunk_(nullptr) {}

  inline Slice key(size_t i) const { return keys_[i]; }
  inline size_t keysize() const { return keys_.size(); }

  inline Slice val(size_t i) const { return vals_[i]; }

  inline size_t size() const { return keys_.size(); }

  inline const Hash signature() const { return signature_; }

  inline uint64_t time() const { return time_; }

  inline uint64_t txnid() const { return txnid_; }

 private:
  void Parse();

  const Chunk* chunk_;
  std::vector<Slice> keys_;
  std::vector<Slice> vals_;
  Hash signature_;
  uint64_t time_;
  uint64_t txnid_;
};

class Logger {
 public:
  Logger(const char* wal_path, const char* idx_path) {
    wal_ = open(wal_path, O_CREAT|O_RDWR, 0600);
    idx_ = open(idx_path, O_CREAT|O_RDWR, 0600);
    start_ = 0;
    end_ = 0;
    seq_ = 0;
  }
  ~Logger() {
    close(wal_);
    close(idx_);
  }

  std::string read(uint64_t seq, uint64_t* block) {
    auto idxEntrySize = sizeof(uint64_t) * 4;
    auto idxOffset = seq * idxEntrySize;
    byte_t buffer[idxEntrySize];
    pread(idx_, buffer, idxEntrySize, idxOffset);
    *block = *reinterpret_cast<const uint64_t*>(buffer + sizeof(uint64_t));
    auto walOffset = *reinterpret_cast<const uint64_t*>(buffer + sizeof(uint64_t)*2);
    auto end = *reinterpret_cast<const uint64_t*>(buffer + sizeof(uint64_t)*3);
    auto walSize = end - walOffset;
    byte_t buf[walSize];
    pread(wal_, buf, walSize, walOffset);
    return std::string(reinterpret_cast<const char*>(buf), walSize);
  }

  void log(const byte_t* data, size_t size) {
    write(wal_, data, size);
  }

  void advance(uint64_t size) {
    std::unique_lock<std::mutex> writelock(mu_);
    end_ += size;
  }

  void index(uint64_t block) {
    uint64_t start, end, seq;
    {
      std::unique_lock<std::mutex> writelock(mu_);
      start = start_;
      end = end_;
      start_ = end_;
      seq = seq_++;
    }
    auto size = sizeof(uint64_t) * 4;
    std::unique_ptr<byte_t[]> entry(new byte_t[size]);
    memcpy(&entry[0], &seq, sizeof(uint64_t));
    memcpy(&entry[sizeof(uint64_t)], &block, sizeof(uint64_t));
    memcpy(&entry[sizeof(uint64_t)*2], &start, sizeof(uint64_t));
    memcpy(&entry[sizeof(uint64_t)*3], &end, sizeof(uint64_t));
    write(idx_, entry.get(), size);
  }

 private:
  int wal_;
  int idx_;

  uint64_t seq_;
  uint64_t start_;
  uint64_t end_;
  std::mutex mu_;
};

}

#endif  // LOGGER_H_
