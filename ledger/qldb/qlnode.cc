#include "ledger/qldb/qlnode.h"

namespace ledgebase {

namespace qldb {

Chunk Document::Encode(const Slice& key,
                       const Slice& val,
                       const BlockAddress& addr,
                       const MetaData& meta) {
  auto ledger_nm_bytes = uint32_t(addr.ledger_name.len());
  auto key_bytes = key.len();
  auto entry_bytes = key.len() + val.len() + sizeof(uint32_t) * 2;
  size_t bytes = Document::fixed_length + key.len() + val.len()
      + ledger_nm_bytes;

  Chunk chunk(ChunkType::kDocument, bytes);
  memcpy(chunk.m_data(), &ledger_nm_bytes, sizeof(uint32_t));
  size_t offset = sizeof(uint32_t);
  memcpy(chunk.m_data() + offset, addr.ledger_name.data(), ledger_nm_bytes);
  offset += ledger_nm_bytes;
  memcpy(chunk.m_data() + offset, &addr.seq_no, sizeof(uint64_t));
  offset += sizeof(uint64_t);
  memcpy(chunk.m_data() + offset, &entry_bytes, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(chunk.m_data() + offset, &key_bytes, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(chunk.m_data() + offset, key.data(), key_bytes);
  offset += key_bytes;
  memcpy(chunk.m_data() + offset, val.data(), val.len());
  offset += val.len();
  memcpy(chunk.m_data() + offset, &meta.doc_seq, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(chunk.m_data() + offset, &meta.version, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(chunk.m_data() + offset, &meta.time, sizeof(uint64_t));

  return std::move(chunk);
}

void Document::PrecomputeOffset() {
  auto ldg_bytes = *reinterpret_cast<const uint32_t*>(chunk_->data());
  size_t offset = sizeof(uint32_t);
  addr_.ledger_name = Slice(chunk_->data() + offset, ldg_bytes);
  offset += ldg_bytes;
  addr_.seq_no = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
  offset += sizeof (uint64_t);
  auto entry_bytes = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);
  auto key_bytes = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);
  data_.key = Slice(chunk_->data() + offset, key_bytes);
  offset += key_bytes;
  data_.val = Slice(chunk_->data() + offset, entry_bytes - key_bytes -
      sizeof(uint32_t) * 2);
  offset +=  entry_bytes - key_bytes - sizeof(uint32_t) * 2;
  metadata_.doc_seq = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);
  metadata_.version = *reinterpret_cast<const uint32_t*>(chunk_->data() + offset);
  offset += sizeof(uint32_t);
  metadata_.time = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
}

Chunk QLBlock::Encode(const BlockAddress& addr,
                      const Statement& statement,
                      const uint64_t block_time,
                      const Hash& block_hash,
                      const Hash& previous_hash,
                      const std::vector<Chunk>& documents) {
  auto ledger_nm_bytes = addr.ledger_name.len();
  size_t bytes = QLBlock::fixed_length + ledger_nm_bytes + sizeof(uint32_t);
  for (auto& doc : documents) {
    bytes += doc.numBytes();
  }
  auto num_entries = documents.size();

  Chunk chunk(ChunkType::kQLBlock, bytes);
  memcpy(chunk.m_data(), block_hash.value(), Hash::kByteLength);
  size_t offset = Hash::kByteLength;
    memcpy(chunk.m_data() + offset, previous_hash.value(), Hash::kByteLength);
  offset += Hash::kByteLength;
    memcpy(chunk.m_data() + offset, &block_time, sizeof(uint64_t));
  offset += sizeof(uint64_t);
    memcpy(chunk.m_data() + offset, &ledger_nm_bytes, sizeof(uint32_t));
  offset += sizeof(uint32_t);
    memcpy(chunk.m_data() + offset, addr.ledger_name.data(), ledger_nm_bytes);
  offset += ledger_nm_bytes;
    memcpy(chunk.m_data() + offset, &addr.seq_no, sizeof(uint64_t));
  offset += sizeof(uint64_t);
    memcpy(chunk.m_data() + offset, &statement.type, 1);
  offset += 1;
    memcpy(chunk.m_data() + offset, &statement.time, sizeof(uint64_t));
  offset += sizeof(uint64_t);
    memcpy(chunk.m_data() + offset, &num_entries, sizeof(uint32_t));
  offset += sizeof(uint32_t);
    for (size_t i = 0; i < documents.size(); ++i) {
    memcpy(chunk.m_data() + offset, documents[i].head(),
        documents[i].numBytes());
    offset += documents[i].numBytes();
      }
  return std::move(chunk);
}

void QLBlock::UpdateHash(Chunk* block, const Hash& block_hash) {
  memcpy(block->m_data(), block_hash.value(), Hash::kByteLength);
}

void QLBlock::PrecomputeOffset() {
  block_hash_ =  Hash(chunk_->data());
  size_t offset = Hash::kByteLength;
  previous_hash_ = Hash(chunk_->data() + offset);
  offset += Hash::kByteLength;
  block_time_ = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
  offset += sizeof(uint64_t);
  auto ledger_nm_bytes = *reinterpret_cast<const uint32_t*>(
      chunk_->data() + offset);
  offset += sizeof(uint32_t);
  addr_.ledger_name = Slice(chunk_->data() + offset, ledger_nm_bytes);
  offset += ledger_nm_bytes;
  addr_.seq_no = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
  offset += sizeof(uint64_t);
  statement_.type = *reinterpret_cast<const byte_t*>(chunk_->data() + offset);
  offset += 1;
  statement_.time = *reinterpret_cast<const uint64_t*>(chunk_->data() + offset);
  offset += sizeof(uint64_t);
  auto num_entries = *reinterpret_cast<const uint32_t*>(
      chunk_->data() + offset);
  offset += sizeof(uint32_t);
  for (size_t i = 0; i < num_entries; ++i) {
    auto num_bytes = *reinterpret_cast<const uint32_t*>(
        chunk_->data() + offset);
    documents_.emplace_back(chunk_->data() + offset);
    offset += num_bytes;
  }
}

}  // namespace qldb

}  // namespace ledgebase
