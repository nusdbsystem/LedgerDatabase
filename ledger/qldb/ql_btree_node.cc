#include "ledger/qldb/ql_btree_node.h"

#include "ledger/qldb/bplus_config.h"

namespace ledgebase {

namespace qldb {

size_t BPlusConfig::fanout_ = 10;
size_t BPlusConfig::leaf_fanout_ = 10;

std::unique_ptr<const QLBTreeNode>
    QLBTreeNode::CreateFromChunk(const Chunk* chunk) {
  switch (chunk->type()) {
    case ChunkType::kMeta:
      return std::unique_ptr<QLBTreeNode>(new QLBTreeMeta(chunk));
    case ChunkType::kMap:
      return std::unique_ptr<QLBTreeNode>(new QLBTreeMap(chunk));
    default:
      return nullptr;
  }
}

void QLBTreeMeta::PrecomputeOffset() {
  // number of elements, number of entries
  level_ = *reinterpret_cast<const uint32_t*>(chunk_->data() +
      sizeof(uint64_t));
  size_t n_entries = *reinterpret_cast<const uint32_t*>(chunk_->data() +
      sizeof(uint64_t) + sizeof(uint32_t));
  size_t offset = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t);
  for (size_t i = 0; i < n_entries; ++i) {
    size_t key_bytes =
        *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
    offset += sizeof(uint32_t);

    keys_.emplace_back(chunk_->data() + offset, key_bytes);
    offset += key_bytes;

    num_elems_.emplace_back(
        *reinterpret_cast<const uint64_t*>(chunk_->data() + offset));
    offset += sizeof(uint64_t);
  }
  num_elems_.emplace_back(
      *reinterpret_cast<const uint64_t*>(chunk_->data() + offset));
  offset += sizeof(uint64_t);
}

void QLBTreeMap::PrecomputeOffset() {
  // number of elements, number of entries
  level_ = *reinterpret_cast<const uint32_t*>(chunk_->data() +
      sizeof(uint64_t));
  size_t n_entries = *reinterpret_cast<const uint32_t*>(chunk_->data() +
      sizeof(uint64_t) + sizeof(uint32_t));
  size_t offset = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t);
  for (size_t i = 0; i < n_entries; ++i) {
    size_t n_bytes =
        *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);

    auto key_byte_size = offset + sizeof(uint32_t);
    size_t key_bytes =
        *reinterpret_cast<const uint32_t*>(chunk_->data() + key_byte_size);

    auto key_offset = key_byte_size + sizeof(uint32_t);
    keys_.emplace_back(chunk_->data() + key_offset, key_bytes);

    auto val_offset = key_offset + key_bytes;
    auto val_bytes = n_bytes - key_bytes - sizeof(uint32_t) * 2;
    vals_.emplace_back(chunk_->data() + val_offset, val_bytes);

    offset += n_bytes;
  }
  size_t next_bytes =
      *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);

  next_ = Slice(chunk_->data() + offset, next_bytes);
}

Chunk QLBTreeMeta::Encode(size_t level, const std::vector<Slice>& keys,
    const std::vector<uint64_t>& num_elems) {
  uint32_t num_entries = keys.size();
  uint64_t num_elem = 0;
  for (auto& ne : num_elems) {
    num_elem += ne;
  }
  size_t capacity = sizeof(uint64_t) + sizeof(uint32_t) * 2;

  // calculate size
  for (size_t i = 0; i < keys.size(); ++i) {
    capacity += sizeof(uint32_t) + keys[i].len() + sizeof(uint64_t);
  }
  capacity += sizeof(uint64_t);
  
  // create chunk
  Chunk chunk(ChunkType::kMeta, capacity);
  memcpy(chunk.m_data(), &num_elem, sizeof(uint64_t));
  memcpy(chunk.m_data() + sizeof(uint64_t), &level, sizeof(uint32_t));
  memcpy(chunk.m_data() + sizeof(uint64_t) + sizeof(uint32_t), &num_entries,
      sizeof(uint32_t));
  size_t offset = sizeof(uint64_t) + sizeof(uint32_t) * 2;
  for (size_t i = 0; i < num_entries; ++i) {
    size_t num_key_bytes = keys[i].len();
    memcpy(chunk.m_data() + offset, &num_key_bytes, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(chunk.m_data() + offset, keys[i].data(), num_key_bytes);
    offset += num_key_bytes;

    memcpy(chunk.m_data() + offset, &num_elems[i], sizeof(uint64_t));
    offset += sizeof(uint64_t);
  }
  memcpy(chunk.m_data() + offset, &num_elems[num_elems.size() - 1],
      sizeof(uint64_t));
  return std::move(chunk);
}

Chunk QLBTreeMap::Encode(size_t level, const std::vector<Slice>& keys,
    const std::vector<Slice>& vals, const Slice& next) {
  uint32_t num_entries = keys.size();
  uint64_t num_elem = keys.size();
  size_t capacity = sizeof(uint64_t) + sizeof(uint32_t) * 2;

  // calculate size
  for (size_t i = 0; i < keys.size(); ++i) {
    capacity += sizeof(uint32_t) * 2 + keys[i].len() + vals[i].len();
  }
  capacity += sizeof(uint32_t) + next.len();
  
  // create chunk
  Chunk chunk(ChunkType::kMap, capacity);
  memcpy(chunk.m_data(), &num_elem, sizeof(uint64_t));
  memcpy(chunk.m_data() + sizeof(uint64_t), &level, sizeof(uint32_t));
  memcpy(chunk.m_data() + sizeof(uint64_t) + sizeof(uint32_t), &num_entries,
      sizeof(uint32_t));
  size_t offset = sizeof(uint64_t) + sizeof(uint32_t) * 2;
  for (size_t i = 0; i < num_entries; ++i) {
    uint32_t n_bytes = sizeof(uint32_t) * 2 + keys[i].len() + vals[i].len();
    memcpy(chunk.m_data() + offset, &n_bytes, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    size_t key_bytes = keys[i].len();
    memcpy(chunk.m_data() + offset, &key_bytes, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(chunk.m_data() + offset, keys[i].data(), key_bytes);
    offset += key_bytes;

    memcpy(chunk.m_data() + offset, vals[i].data(), vals[i].len());
    offset += vals[i].len();
  }
  size_t next_bytes = next.len();
  memcpy(chunk.m_data() + offset, &next_bytes, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(chunk.m_data() + offset, next.data(), next_bytes);
  return std::move(chunk);
}

Chunk QLBTreeMap::EmptyMap() {
  auto capacity = sizeof(uint64_t) + sizeof(uint32_t) * 3;
  Chunk chunk(ChunkType::kMap, capacity);
  uint64_t num_elem = 0;
  uint32_t num_entries = 0;
  uint32_t level = 0;
  uint32_t next_bytes = 0;

  memcpy(chunk.m_data(), &num_elem, sizeof(uint64_t));
  auto offset = sizeof(uint64_t);

  memcpy(chunk.m_data() + offset, &level, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(chunk.m_data() + offset, &num_entries, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(chunk.m_data() + offset, &next_bytes, sizeof(uint32_t));
  return std::move(chunk);
}

size_t QLBTreeMeta::BinarySearch(const Slice& key,
    int start, int end) const {
  if (end <= start) {
    if (start > int(numEntries())) start = numEntries();
    while (start < int(numEntries()) && key > keys_[start]) {
      ++start;
    }
    return start;
  }
  auto pivot = (end + start) / 2;
  if (key == keys_[pivot]) {
    return pivot;
  } else if (key < keys_[pivot]) {
    return BinarySearch(key, start, pivot - 1);
  } else {
    return BinarySearch(key, pivot + 1, end);
  }
}

QLBTreeInsertResult QLBTreeMeta::UpdateChild(size_t index,
    const QLBTreeInsertResult& retval, QLBTreeDelta* bp_delta,
    const std::string& prefix) const {
  // reorder meta node
  std::vector<Slice> new_keys;
  std::vector<uint64_t> new_elems;
  for (size_t i = 0; i < numEntries(); ++i) {
    if (i == index) {
      if (retval.isSplit) {
        new_keys.emplace_back(retval.split_key);
      }
      new_keys.emplace_back(keys_[i]);
      new_elems.insert(new_elems.end(), retval.num_elems.begin(),
          retval.num_elems.end());
    } else {
      new_keys.emplace_back(keys_[i]);
      new_elems.emplace_back(num_elems_[i]);
    }
  }
  if (index >= numEntries()) {
    if (retval.isSplit) {
      new_keys.emplace_back(retval.split_key);
    }
    new_elems.insert(new_elems.end(), retval.num_elems.begin(),
        retval.num_elems.end());
  } else {
    new_elems.emplace_back(num_elems_[numEntries()]);
  }

  // create new metanode
  QLBTreeInsertResult result;
  auto thisid = GetChildElems(numEntries()) == 0?
      new_keys[new_keys.size() - 1].ToString() :
      "_INFI_";
  if (new_keys.size() <= BPlusConfig::fanout()) {
    Chunk chunk1 = QLBTreeMeta::Encode(GetLevel(), new_keys, new_elems);
    auto id = prefix + std::to_string(GetLevel()) + "|" + thisid;
    auto res_num_elems = *reinterpret_cast<const uint64_t*>(chunk1.data());
    result = {false, "", GetLevel(), {res_num_elems}};
    bp_delta->CreateChunk(id, std::move(chunk1));
  } else {
    size_t split_idx = BPlusConfig::ComputeSplitIndex(new_keys.size());
    auto split_key = new_keys[split_idx - 1].ToString();
    std::vector<Slice> newkeys1(new_keys.begin(), new_keys.begin() + split_idx);
    std::vector<Slice> newkeys2(new_keys.begin() + split_idx, new_keys.end());
    std::vector<uint64_t> newelems1(new_elems.begin(),
        new_elems.begin() + split_idx);
    newelems1.emplace_back(0);
    std::vector<uint64_t> newelems2(new_elems.begin() + split_idx,
        new_elems.end());
    Chunk chunk1 = QLBTreeMeta::Encode(GetLevel(), newkeys1, newelems1);
    Chunk chunk2 = QLBTreeMeta::Encode(GetLevel(), newkeys2, newelems2);
    auto id1 = prefix + std::to_string(GetLevel()) + "|" +
        split_key;
    auto id2 = prefix + std::to_string(GetLevel()) + "|" + thisid;
    auto res_num_elems1 = *reinterpret_cast<const uint64_t*>(chunk1.data());
    auto res_num_elems2 = *reinterpret_cast<const uint64_t*>(chunk2.data());
    result = {true, split_key, GetLevel(), {res_num_elems1, res_num_elems2}};

    bp_delta->CreateChunk(id1, std::move(chunk1));
    bp_delta->CreateChunk(id2, std::move(chunk2));
  }
  return std::move(result);
}

}  // namespace qldb

}  // namespace ledgebase