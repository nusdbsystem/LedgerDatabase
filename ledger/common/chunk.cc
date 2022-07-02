#include "ledger/common/chunk.h"

#include <utility>

namespace ledgebase {

Chunk::Chunk(ChunkType type, uint32_t capacity)
    : own_(new unsigned char[kMetaLength + capacity]) {
  head_ = own_.get();
  *reinterpret_cast<uint32_t*>(&own_[kNumBytesOffset]) = kMetaLength + capacity;
  *reinterpret_cast<ChunkType*>(&own_[kChunkTypeOffset]) = type;
}

Chunk::Chunk(std::unique_ptr<unsigned char[]> head) noexcept : own_(std::move(head)) {
  head_ = own_.get();
}

}  // namespace ledgebase
