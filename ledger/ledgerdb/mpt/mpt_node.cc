#include "ledger/ledgerdb/mpt/mpt_node.h"

namespace ledgebase {

namespace ledgerdb {

std::unique_ptr<const MPTNode>
    MPTNode::CreateFromChunk(const Chunk* chunk) {
  switch (chunk->type()) {
    case ChunkType::kMPTFull:
      return std::unique_ptr<MPTNode>(new MPTFullNode(chunk));
    case ChunkType::kMPTShort:
      return std::unique_ptr<MPTNode>(new MPTShortNode(chunk));
    case ChunkType::kMPTHash:
      return std::unique_ptr<MPTNode>(new MPTHashNode(chunk));
    case ChunkType::kMPTValue:
      return std::unique_ptr<MPTNode>(new MPTValueNode(chunk));
    default:
      return std::unique_ptr<MPTNode>(new MPTNilNode(chunk));
  }
}

uint64_t MPTNode::numElements() const {
  switch (chunk_->type()) {
    case ChunkType::kMPTFull:
    case ChunkType::kMPTShort:
      return *reinterpret_cast<const uint64_t*>(
          chunk_->data());
    case ChunkType::kMPTValue:
      return uint64_t(1);
    default:
      return uint64_t(0);
  }
}

void MPTFullNode::PrecomputeOffset() {
  size_t offset = sizeof(uint64_t);
  for (size_t i = 0; i < 17; ++i) {
    children_.emplace_back(chunk_->data() + offset);
    offset += kHashSize;
  }
}

void MPTShortNode::PrecomputeOffset() {
  size_t num_key_offset = sizeof(uint64_t);
  size_t key_len = *reinterpret_cast<const uint32_t*>(
      chunk_->data() + num_key_offset);

  size_t key_offset = num_key_offset + sizeof(uint32_t);
  size_t val_offset = key_offset + key_len;

  key_ = std::string(reinterpret_cast<const char*>(chunk_->data()
      + key_offset), key_len);
  val_ = Chunk(chunk_->data() + val_offset);
}

void MPTHashNode::PrecomputeOffset() {
  hash_ = Hash(chunk_->data());
}

void MPTValueNode::PrecomputeOffset() {
  size_t num_bytes = chunk_->numBytes();
  size_t num_val_bytes = num_bytes - Chunk::kMetaLength;
  val_ = std::string(reinterpret_cast<const char*>(chunk_->data()),
      num_val_bytes);
}

Chunk MPTFullNode::Encode(const std::vector<Chunk>& nodes) {
  if (nodes.size() != 17) return Chunk();
  uint64_t num_elem = 0;
  size_t capacity = sizeof(uint64_t);

  // calculate size
  for (size_t i = 0; i < 17; ++i) {
    capacity += i == 16? nodes[i].numBytes() : kHashSize;
    if (nodes[i].type() == ChunkType::kMPTNil) {
    } else if (nodes[i].type() == ChunkType::kMPTValue) {
      ++num_elem;
    } else {
      num_elem += *reinterpret_cast<const uint64_t*>(nodes[i].data());
    }
  }

  // create chunk
  Chunk chunk(ChunkType::kMPTFull, capacity);
  memcpy(chunk.m_data(), &num_elem, sizeof(uint64_t));
  size_t offset = sizeof(uint64_t);
  for (size_t i = 0; i < 16; ++i) {
    Hash hash = nodes[i].hash();
    memcpy(chunk.m_data() + offset, &kHashSize, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    ChunkType type = ChunkType::kMPTHash;
    memcpy(chunk.m_data() + offset, &type, sizeof(ChunkType));
    offset += sizeof(ChunkType);
    memcpy(chunk.m_data() + offset, hash.value(), Hash::kByteLength);
    offset += Hash::kByteLength;
  }
  memcpy(chunk.m_data() + offset, nodes[16].head(),
         nodes[16].numBytes());
  return std::move(chunk);
}

Chunk MPTShortNode::Encode(const std::string& key, const Chunk* node) {
  uint64_t num_elem = (node->type() == ChunkType::kMPTValue) ?
      1 : *reinterpret_cast<const uint64_t*>(node->data());
  size_t key_size = key.length();
  size_t num_elem_offset = 0;
  size_t key_size_offset = num_elem_offset + sizeof(uint64_t);
  size_t key_offset = key_size_offset + sizeof(uint32_t);
  size_t node_offset = key_offset + key_size;
  size_t capacity = 0;

  if (node->type() == ChunkType::kMPTValue) {
    capacity = node_offset + node->numBytes();
  } else {
    capacity = node_offset + kHashSize;
  }

  Chunk chunk(ChunkType::kMPTShort, capacity);
  memcpy(chunk.m_data() + num_elem_offset, &num_elem, sizeof(uint64_t));
  memcpy(chunk.m_data() + key_size_offset, &key_size, sizeof(uint32_t));
  memcpy(chunk.m_data() + key_offset,
      reinterpret_cast<const unsigned char*>(key.data()), key_size);
  if (node->type() == ChunkType::kMPTValue) {
    memcpy(chunk.m_data() + node_offset, node->head(), node->numBytes());
  } else {
    ChunkType type = ChunkType::kMPTHash;
    Hash hash = node->hash();

    memcpy(chunk.m_data() + node_offset, &kHashSize, sizeof(uint32_t));
    memcpy(chunk.m_data() + node_offset + sizeof(uint32_t), &type,
        sizeof(ChunkType));
    memcpy(chunk.m_data() + node_offset + sizeof(uint32_t) + sizeof(ChunkType),
        hash.value(), Hash::kByteLength);
  }
  return std::move(chunk);
}

Chunk MPTHashNode::Encode(const Chunk* child) {
  Chunk chunk(ChunkType::kMPTHash, Hash::kByteLength);
  Hash hash = child->hash();
  memcpy(chunk.m_data(), hash.value(), Hash::kByteLength);
  return std::move(chunk);
}

Chunk MPTValueNode::Encode(const std::string& value) {
  Chunk chunk(ChunkType::kMPTValue, value.size());
  memcpy(chunk.m_data(),
      reinterpret_cast<const unsigned char*>(value.c_str()), value.size());
  return std::move(chunk);
}

Chunk MPTNilNode::Encode() {
  Chunk chunk(ChunkType::kMPTNil, 0);
  return std::move(chunk);
}

Chunk MPTFullNode::setChildAtIndex(size_t index, const Chunk* child,
    const Chunk* o_child) const {
  // num elements
  uint64_t new_elem = 0, old_elem = 0;
  if (child->type() == ChunkType::kMPTNil) {
  } else if (child->type() == ChunkType::kMPTValue) {
    new_elem = 1;
  } else {
    new_elem = *reinterpret_cast<const uint64_t*>(child->data());
  }
  if (o_child->type() == ChunkType::kMPTNil) {
  } else if (o_child->type() == ChunkType::kMPTValue) {
    old_elem = 1;
  } else {
    old_elem = *reinterpret_cast<const uint64_t*>(o_child->data());
  }
  uint64_t num_elem = *reinterpret_cast<const uint64_t*>(chunk_->data()) +
      new_elem - old_elem;

  Chunk value = index == 16? Chunk(child->head()) :
                             MPTHashNode::Encode(child);

  // capacity
  uint32_t capacity = chunk_->numBytes() - Chunk::kMetaLength +
                  value.numBytes() - children_[index].numBytes();

  Chunk chunk(ChunkType::kMPTFull, capacity);
  memcpy(chunk.m_data(), &num_elem, sizeof(uint64_t));
  if (index > 0) {
    // if not the first elem, copy the previous elem from current chunk
    memcpy(chunk.m_data() + sizeof(uint64_t),
        chunk_->data() + sizeof(uint64_t), kHashSize * index);
  }
  // copy the modified elem
  memcpy(chunk.m_data() + sizeof(uint64_t) + kHashSize * index, value.head(),
      value.numBytes());
  if (index < 16) {
    // if not the last elem, copy the tail elems from current chunk
    // capacity unchanged
    memcpy(chunk.m_data() + sizeof(uint64_t) + kHashSize * (index + 1),
           chunk_->data() + sizeof(uint64_t) + kHashSize * (index + 1),
           capacity - sizeof(uint64_t) - kHashSize * (index + 1));
  }
  return std::move(chunk);
}

}  // namespace ledgerdb

}  // namespace ledgebase
