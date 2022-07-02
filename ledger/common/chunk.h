#ifndef CHUNK_H_
#define CHUNK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ledger/common/hash.h"

namespace ledgebase {

enum class ChunkType : unsigned char {
  kMPTFull = 0,
  kMPTShort = 1,
  kMPTHash = 2,
  kMPTValue = 3,
  kMPTNil = 4,

  kMeta = 5,
  kMap = 6,
  kList = 7,

  kDocument = 8,
  kQLBlock = 9,

  kBlock = 10,
  kValue = 11,

  kTxnLog = 12,

  First = kMPTFull,
  Last = kValue,

  kInvalid = 99
};

enum class QueryType : unsigned char {
  kRead = 0,
  kUpdate = 1,
  kDelete = 2
};

class Chunk {
 public:
  Chunk() : Chunk(nullptr) {}
  Chunk(ChunkType type, uint32_t capacity);
  explicit Chunk(const unsigned char* head) noexcept : head_(head) {}
  explicit Chunk(std::unique_ptr<unsigned char[]> head) noexcept;
  ~Chunk() = default;

  // Moveable and Non-copyable
  Chunk(const Chunk&) = delete;
  Chunk(Chunk&&) = default;
  Chunk& operator=(const Chunk&) = delete;                                    
  Chunk& operator=(Chunk&&) = default;

  /*
   * Chunk format:
   * | uint32_t  | ChunkType | Bytes |
   * | num_bytes | type      | data  |
   */
  static constexpr size_t kNumBytesOffset = 0;
  static constexpr size_t kChunkTypeOffset = kNumBytesOffset + sizeof(uint32_t);
  static constexpr size_t kMetaLength = kChunkTypeOffset + sizeof(ChunkType);

  inline bool empty() const noexcept { return head_ == nullptr; }
  inline bool own() const noexcept { return own_.get() == nullptr; }
  // total number of bytes
  inline uint32_t numBytes() const noexcept {
    return *reinterpret_cast<const uint32_t*>(head_ + kNumBytesOffset);
  }
  // type of the chunk
  inline ChunkType type() const noexcept {
    return *reinterpret_cast<const ChunkType*>(head_ + kChunkTypeOffset);
  }
  // number of bytes used to store actual data
  inline uint32_t capacity() const noexcept { return numBytes() - kMetaLength; }

  // pointer to the chunk
  inline const unsigned char* head() const noexcept { return head_; }

  // pointer to actual data
  inline const unsigned char* data() const noexcept { return head_ + kMetaLength; }

  // pointer to mutable data
  inline unsigned char* m_data() const noexcept {
      return &own_[kMetaLength];
  };

  // chunk hash
  inline const Hash& hash() const {
    return hash_.empty() ? forceHash() : hash_;
  }
  // force to re-compute chunk hash
  inline const Hash& forceHash() const {
    hash_ = Hash::ComputeFrom(head_, numBytes());
    return hash_;
  }

 private:
  // own the chunk if created by itself
  std::unique_ptr<unsigned char[]> own_;
  // read-only chunk if passed from chunk storage
  const unsigned char* head_ = nullptr;
  mutable Hash hash_;
};

}  // namespace ledgebase

#endif  // CHUNK_H_
