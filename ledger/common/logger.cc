#include "ledger/common/logger.h"

namespace ledgebase {

Chunk TxnEntry::Encode(uint64_t time, const std::vector<std::string>& keys,
      const std::vector<std::string>& vals, const Hash& signature,
      uint64_t txnid) {
  uint32_t totalSize = sizeof(uint32_t) + Hash::kByteLength +
                       2*sizeof(uint64_t);
  uint32_t numEntries = keys.size();
  for (size_t i = 0; i < keys.size(); ++i) {
    totalSize += sizeof(uint32_t) * 2 + keys[i].size() + vals[i].size();
  }
  Chunk chunk(ChunkType::kTxnLog, totalSize);
  auto offset = 0;

  memcpy(chunk.m_data() + offset, &numEntries, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (size_t i = 0; i < keys.size(); ++i) {
    auto keySize = keys[i].size();
    memcpy(chunk.m_data() + offset, &keySize, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(chunk.m_data() + offset, keys[i].c_str(), keys[i].size());
    offset += keys[i].size();

    auto valSize = vals[i].size();
    memcpy(chunk.m_data() + offset, &valSize, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(chunk.m_data() + offset, vals[i].c_str(), vals[i].size());
    offset += vals[i].size();
  }

  memcpy(chunk.m_data() + offset, signature.value(), Hash::kByteLength);
  offset += Hash::kByteLength;

  memcpy(chunk.m_data() + offset, &time, sizeof(uint64_t));
  offset += sizeof(uint64_t);

  memcpy(chunk.m_data() + offset, &txnid, sizeof(uint64_t));

  return std::move(chunk);
}

void TxnEntry::Parse() {
  size_t offset = 0;
  auto numEntries = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);
  for (size_t i = 0; i < numEntries; ++i) {
    auto keySize = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
    offset += sizeof(uint32_t);
    Slice key(chunk_->data() + offset, keySize);
    offset += keySize;
    auto valSize = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
    offset += sizeof(uint32_t);
    Slice val(chunk_->data() + offset, valSize);
    offset += valSize;
    keys_.emplace_back(key);
    vals_.emplace_back(val);
  }
  signature_ = Hash(chunk_->data() + offset);
  offset += Hash::kByteLength;
  time_ = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
  offset += sizeof(uint64_t);
  txnid_ = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
}

std::vector<TxnEntry> TxnEntry::ParseTxnEntries(const std::string& data) {
  std::vector<TxnEntry> result;
  Slice slice(data);
  size_t offset = 0;
  while (offset < data.size()) {
    Chunk chunk(slice.data() + offset);
    TxnEntry entry(&chunk);
    offset += chunk.numBytes();
    result.emplace_back(entry);
  }
  return result;
}

}
