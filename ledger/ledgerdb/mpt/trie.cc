#include "ledger/ledgerdb/mpt/trie.h"
#include "ledger/ledgerdb/mpt/mpt_config.h"

namespace ledgebase {

namespace ledgerdb {

Chunk Trie::kNilChunk = MPTNilNode::Encode();

Trie::Trie(DB* db, const Hash& root_hash) noexcept : db_(db) {
  mpt_delta_.reset(new MPTDelta(db));
  SetNodeForHash(root_hash);
}

Trie::Trie(DB* db, const std::vector<std::string>& keys,
    const std::vector<std::string>& vals) noexcept : db_(db) {
  mpt_delta_.reset(new MPTDelta(db));
  Chunk nil_chunk = MPTNilNode::Encode();
  root_node_ = MPTNode::CreateFromChunk(&nil_chunk);
  Hash root_hash = Set(keys, vals);
  SetNodeForHash(root_hash);
}

std::string Trie::Get(const std::string& key) const {
  std::string encoded_key = MPTConfig::KeybytesToHex(key);
  std::string result = TryGet(root_node_->chunk(), encoded_key, 0);
  return std::move(result);
}

std::string Trie::TryGet(const Chunk* node, const std::string& key,
    const size_t pos) const {
  switch (node->type()) {
    case ChunkType::kMPTFull:
    {
      MPTFullNode full_node(node);
      auto index = (size_t) key[pos];
      auto child = full_node.getChildAtIndex(index);
      return TryGet(&child, key, pos + 1);
    }
    case ChunkType::kMPTShort:
    {
      MPTShortNode short_node(node);
      size_t key_len = short_node.getKey().length();
      if (key.length() - pos < key_len ||
          short_node.getKey() != key.substr(pos, key_len)) {
        return "";
      } else {
        Chunk child = short_node.childNode();
        return TryGet(&child, key, pos + key_len);
      }
    }
    case ChunkType::kMPTHash:
    {
      auto child_node = db_->Get(Hash(node->data()));
      if (child_node->empty()) {
        return TryGet(&kNilChunk, key, pos);
      } else {
        return TryGet(child_node, key, pos);
      }
    }
    case ChunkType::kMPTValue:
    {
      auto value_node = MPTValueNode(node);
      return value_node.getVal();
    }
    default:
      return "";
  }
}

Hash Trie::Set(const std::string& key, const std::string& val) const {
  Chunk new_root(root_node_->head());
  std::string encoded_key = MPTConfig::KeybytesToHex(key);
  Chunk val_chunk = MPTValueNode::Encode(val);
  new_root = Insert(&new_root, encoded_key, &val_chunk);
  Hash new_root_hash = new_root.hash().Clone();
  mpt_delta_->Commit(&new_root);
  mpt_delta_->Clear();
  return new_root_hash;
}

Hash Trie::Set(const std::vector<std::string>& keys,
    const std::vector<std::string>& vals) const {
  if (keys.size() == 0) return root_node_->hash();
  Chunk new_root(root_node_->head());
  for (size_t i = 0; i < keys.size(); ++i) {
    std::string encoded_key = MPTConfig::KeybytesToHex(keys[i]);
    Chunk val_chunk = MPTValueNode::Encode(vals[i]);
    new_root = Insert(&new_root, encoded_key, &val_chunk);
  }
  Hash new_root_hash = new_root.hash().Clone();
  mpt_delta_->Commit(&new_root);
  mpt_delta_->Clear();
  return new_root_hash;
}

Chunk Trie::Insert(const Chunk* node, const std::string& key,
    const Chunk* value) const {
  if (key.length() == 0) {
    Chunk root(value->head());
    return std::move(root);
  }
  switch (node->type()) {
    case ChunkType::kMPTFull:
    {
      MPTFullNode full_node(node);
      auto index = (size_t) key[0];
      auto ori_child = full_node.getChildAtIndex(index);
      if (index != 16) {
        ori_child = GetHashNodeChild(&ori_child);
      }
      std::string new_key = key.substr(1);
      auto new_child = Insert(&ori_child, new_key, value);
      Chunk new_root;
      new_root = full_node.setChildAtIndex(index, &new_child, &ori_child);
      Hash root_hash = new_root.hash().Clone();
      mpt_delta_->CreateChunk(root_hash, std::move(new_root));
      return mpt_delta_->GetChunk(root_hash);
    }
    case ChunkType::kMPTShort:
    {
      MPTShortNode short_node(node);
      std::string ori_key = short_node.getKey();
      size_t key_len = short_node.getKey().length();
      size_t match_len = MPTConfig::PrefixLen(key, ori_key);
      Chunk new_root;

      if (match_len == key_len) {
        // Whole key matches, update value only
        std::string new_key = key.substr(match_len);
        Chunk child_chunk = short_node.childNode();
        auto new_child = Insert(&child_chunk, new_key, value);
        new_root = MPTShortNode::Encode(short_node.getKey(), &new_child);
      } else {
        // Create branch
        std::vector<Chunk> branch_child;
        auto index_o = (size_t) ori_key[match_len];
        auto index_u = (size_t) key[match_len];
        Chunk nil_chunk = MPTNilNode::Encode();
        for (size_t i = 0; i < 17; i++) {
          branch_child.emplace_back(nil_chunk.head());
        }
        std::string new_ori_key = ori_key.substr(match_len + 1);
        std::string new_key = key.substr(match_len + 1);
        Chunk child_chunk = short_node.childNode();
        if (child_chunk.type() == ChunkType::kMPTHash) {
          child_chunk = GetHashNodeChild(&child_chunk);
        }
        // only count once
        branch_child[index_o] = Insert(&nil_chunk, new_ori_key,
                                       &child_chunk);
        branch_child[index_u] = Insert(&nil_chunk, new_key, value);
        if (match_len == 0) {
          new_root = MPTFullNode::Encode(branch_child);
        } else {
          Chunk branch_node = MPTFullNode::Encode(branch_child);
          std::string common_key = key.substr(0, match_len);
          new_root = MPTShortNode::Encode(common_key, &branch_node);
          mpt_delta_->CreateChunk(branch_node.hash(), std::move(branch_node));
        }
      }
      Hash root_hash = new_root.hash().Clone();
      mpt_delta_->CreateChunk(root_hash, std::move(new_root));
      return mpt_delta_->GetChunk(root_hash);
    }
    case ChunkType::kMPTHash:
    {
      auto child_node = GetHashNodeChild(node);
      return Insert(&child_node, key, value);
    }
    case ChunkType::kMPTNil:
    {
      Chunk chunk = MPTShortNode::Encode(key, value);
      Hash root_hash = chunk.hash().Clone();
      mpt_delta_->CreateChunk(root_hash, std::move(chunk));
      return mpt_delta_->GetChunk(root_hash);
    }
    default:
      return Chunk();
  }
}

Hash Trie::Remove(const std::string& key) const {
  std::string encoded_key = MPTConfig::KeybytesToHex(key);
  std::string prefix;
  auto new_root = TryRemove(root_node_->chunk(), prefix, encoded_key);
  mpt_delta_->Commit(&new_root);
  mpt_delta_->Clear();
  return new_root.hash().Clone();
}

Chunk Trie::TryRemove(const Chunk* node, std::string& prefix,
    std::string& key) const {
  switch (node->type()) {
    case ChunkType::kMPTFull:
    {
      MPTFullNode full_node(node);
      auto index = (size_t) key[0];
      auto ori_child = full_node.getChildAtIndex(index);
      if (index != 16) {
        ori_child = GetHashNodeChild(&ori_child);
      }
      prefix = prefix+key[0];
      key = key.substr(1);
      auto new_child = TryRemove(&ori_child, prefix, key);
      auto new_root = full_node.setChildAtIndex(index, &new_child, &ori_child);

      // check number of children left in full node
      MPTFullNode new_full(&new_root);
      int pos = -1;
      for (size_t i = 0; i < 17; ++i) {
        if (new_full.getChildAtIndex(i).type() != ChunkType::kMPTNil) {
          pos = (pos == -1) ? i : -2;
        }
      }

      // change to short node if only one child left
      if (pos >= 0) {
        auto hash_node = new_full.getChildAtIndex(pos);
        std::string new_key;
        if (size_t(pos) != 16) {
          auto child_node = GetHashNodeChild(&hash_node);
          // combine if child is short, otherwise create as short
          if (child_node.type() == ChunkType::kMPTShort) {
            MPTShortNode child_short(&child_node);
            new_key.resize(child_short.getKey().length() + 1);
            new_key[0] = pos;
            memcpy(&new_key[1], child_short.getKey().data(),
                child_short.getKey().length());
            hash_node = child_short.childNode();
          }
        } else {
          new_key.resize(1);
          new_key[0] = 16;
        }
        new_root = MPTShortNode::Encode(new_key, &hash_node);
      }

      Hash root_hash = new_root.hash().Clone();
      mpt_delta_->CreateChunk(root_hash, std::move(new_root));
      return mpt_delta_->GetChunk(root_hash);
    }
    case ChunkType::kMPTShort:
    {
      MPTShortNode short_node(node);
      std::string ori_key = short_node.getKey();
      size_t key_len = short_node.getKey().length();
      size_t match_len = MPTConfig::PrefixLen(key, ori_key);
      if (match_len < key_len) {
        return Chunk(node->head());
      } else if (match_len == key.length()) {
        return MPTNilNode::Encode();
      }
      prefix += key.substr(0, match_len);
      key = key.substr(match_len);
      Chunk child_node = short_node.childNode();
      auto new_child = TryRemove(&child_node, prefix, key);
      if (new_child.type() == ChunkType::kMPTShort) {
        MPTShortNode new_short(&new_child);
        std::string combined_key = ori_key + new_short.getKey();
        auto grand_child = new_short.childNode();
        Chunk new_root = MPTShortNode::Encode(combined_key,
                                              &grand_child);

        Hash root_hash = new_root.hash().Clone();
        mpt_delta_->CreateChunk(root_hash, std::move(new_root));
        return mpt_delta_->GetChunk(root_hash);
      } else {
        Chunk new_root = MPTShortNode::Encode(short_node.getKey(), &new_child);
        Hash root_hash = new_root.hash().Clone();
        mpt_delta_->CreateChunk(root_hash, std::move(new_root));
        return mpt_delta_->GetChunk(root_hash);
      }
    }
    case ChunkType::kMPTHash:
    {
      auto child_node = GetHashNodeChild(node);
      return TryRemove(&child_node, prefix, key);
    }
    default:
    {
      return MPTNilNode::Encode();
    }
  }
}

Chunk Trie::GetHashNodeChild(const Chunk* hash_node) const {
  if (hash_node->type() != ChunkType::kMPTHash)
    return MPTNilNode::Encode();
  MPTHashNode node(hash_node);

  // Get from written chunks
  Chunk value = mpt_delta_->GetChunk(node.childHash());

  // If not found, load from DB
  if (value.empty()) {
    const Chunk* chunk_ptr = db_->Get(node.childHash());
    if (chunk_ptr == nullptr || chunk_ptr->empty()) {
      return MPTNilNode::Encode();
    }
    return Chunk(chunk_ptr->head()); 
  } else {
    return std::move(value);
  }
}

bool Trie::SetNodeForHash(const Hash& root_hash) {
  if (root_hash == kNilChunk.hash()) {
    root_node_ = MPTNode::CreateFromChunk(&kNilChunk);
    return true;
  }

  auto chunk = db_->Get(root_hash);
  if (chunk->empty()) {
    root_node_ = MPTNode::CreateFromChunk(&kNilChunk);
  } else {
    root_node_ = MPTNode::CreateFromChunk(chunk);
  }
  return true;
}

void MPTDelta::Commit(const Chunk* node) {
  switch (node->type()) {
    case ChunkType::kMPTFull:
    {
      // commit child
      MPTFullNode full_node(node);
      Chunk nil_chunk = MPTNilNode::Encode();
      Hash nil_hash = nil_chunk.hash();
      for (size_t i = 0; i < 17; ++i) {
        Chunk child(full_node.getChildAtIndex(i));
        if (child.type() == ChunkType::kMPTHash) {
          MPTHashNode hash_node(&child);
          if (hash_node.childHash() != nil_hash) {
            Commit(&child);
          }
        }
      }
      // commit current
      Hash hash = node->hash();
      Chunk chunk = GetChunk(hash);
      if (!chunk.empty()) {
        if (hash != Trie::kNilChunk.hash()) {
          db_->Put(hash, chunk);
        }
      }
      break;
    }
    case ChunkType::kMPTShort:
    {
      // commit child
      MPTShortNode short_node(node);
      Chunk short_chunk = short_node.childNode();
      Commit(&short_chunk);
      // commit current
      Hash hash = node->hash();
      Chunk chunk = GetChunk(hash);
      if (!chunk.empty()) {
        if (hash != Trie::kNilChunk.hash()) {
          db_->Put(hash, chunk);
        }
      }
      break;
    }
    case ChunkType::kMPTHash:
    {
      MPTHashNode hash_node(node);
      Chunk chunk = GetChunk(hash_node.childHash());
      if (!chunk.empty()) {
        Commit(&chunk);
      }
      break;
    }
    default:
      break;
  }
}

MPTProof Trie::GetProof(const std::string& key) const {
  MPTProof proof;
  std::string encoded_key = MPTConfig::KeybytesToHex(key);
  TryGetProof(root_node_->chunk(), encoded_key, 0, &proof);
  //std::cout << "blocks " << proof.MapSize() << std::endl;
  return proof;
}

void Trie::TryGetProof(const Chunk* node, const std::string& key,
    const size_t pos, MPTProof* proof) const {
  switch (node->type()) {
    case ChunkType::kMPTFull:
    {
      MPTFullNode full_node(node);
      auto index = (size_t) key[pos];
      proof->AppendProof(node->head(), index);
      auto child = full_node.getChildAtIndex(index);
      return TryGetProof(&child, key, pos + 1, proof);
    }
    case ChunkType::kMPTShort:
    {
      proof->AppendProof(node->head(), 0);
      MPTShortNode short_node(node);
      size_t key_len = short_node.getKey().length();
      if (key.length() - pos < key_len ||
          short_node.getKey() != key.substr(pos, key_len)) {
        proof->SetValue("");
        return;
      } else {
        Chunk child = short_node.childNode();
        return TryGetProof(&child, key, pos + key_len, proof);
      }
    }
    case ChunkType::kMPTHash:
    {
      auto child_node = db_->Get(Hash(node->data()));
      if (child_node->empty()) {
        return TryGetProof(&kNilChunk, key, pos, proof);
      } else {
        return TryGetProof(child_node, key, pos, proof);
      }
    }
    case ChunkType::kMPTValue:
    {
      auto value_node = MPTValueNode(node);
      proof->SetValue(value_node.getVal());
      return;
    }
    default:
      return;
  }
}

bool MPTProof::VerifyProof(const Hash& digest, const std::string& key) const {
  std::string encoded_key = MPTConfig::KeybytesToHex(key);
  size_t pos = 0;
  auto map_size = map_chunks_.size();
  Hash target = digest;

  for (size_t i = 0; i < map_size; ++i) {
    auto nodestr = GetMapChunk(i);
    Chunk chunk(reinterpret_cast<const unsigned char*>(nodestr.c_str()));
    auto calculated = chunk.hash();
    if (target != calculated) {
      return false;
    }

    if (chunk.type() == ChunkType::kMPTFull) {
      ++pos;
      MPTFullNode fullnode(&chunk);
      auto hashchk = fullnode.getChildAtIndex(map_pos_[i]);
      if (map_pos_[i] == 16) {
        MPTValueNode valuenode(&hashchk);
        auto target_val = valuenode.getVal();
        if (target_val != value_) {
          return false;
        }
      } else {
        MPTHashNode hashnode(&hashchk);
        target = hashnode.childHash().Clone();
      }
    } else {
      if (!value_.empty()) {
        MPTShortNode shortnode(&chunk);
        auto child = shortnode.childNode();
        if (child.type() == ChunkType::kMPTHash) {
          MPTHashNode hashnode(&child);
          target = hashnode.childHash().Clone();
        } else {
          MPTValueNode valuenode(&child);
          auto target_val = valuenode.getVal();
          if (target_val != value_) {
            return false;
          }
        }
        pos += shortnode.getKey().length();
      } else {
        MPTShortNode shortnode(&chunk);
        auto target_key = shortnode.getKey();
        auto curr_key = key.substr(pos);
        size_t match_len = MPTConfig::PrefixLen(curr_key, target_key);
        if (match_len != target_key.size()) {
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace ledgerdb

}  // namespace ledgebase
