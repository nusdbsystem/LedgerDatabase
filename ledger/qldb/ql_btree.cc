#include "ledger/qldb/ql_btree.h"

#include "ledger/qldb/bplus_config.h"

namespace ledgebase {

namespace qldb {

Chunk QLBTree::kEmptyMap = QLBTreeMap::EmptyMap();

QLBTree::QLBTree(DB* db, const std::string& prefix) noexcept :
    db_(db), prefix_(prefix) {
  auto chunk = db_->Get(prefix_ + "ROOT");
  if (chunk->empty()) {
    root_node_ = QLBTreeNode::CreateFromChunk(&kEmptyMap);
  } else {
    root_node_ = QLBTreeNode::CreateFromChunk(chunk);
  }
  
  delta_.reset(new QLBTreeDelta(db));
}

Slice QLBTree::Get(const Slice& key) const {
  Slice result = TryGet(root_node_->chunk(), key);
  return std::move(result);
}

Slice QLBTree::TryGet(const Chunk* node, const Slice& key) const {
  switch(node->type()) {
    case ChunkType::kMap:
    {
      //Timer timer;timer.Start();
      QLBTreeMap map_node(node);
      //std::cerr << "map " << map_node.numEntries() << std::endl;
      for (size_t i = 0; i < map_node.numEntries(); ++i) {
        if (key == map_node.GetKey(i)) {
          //Metrics::mapscan += timer.ElapsedMilliseconds(); timer.Reset();
          return map_node.GetVal(i);
        } else if (key < map_node.GetKey(i)) {
          break;
        }
      }
      //Metrics::mapscan += timer.ElapsedMilliseconds(); timer.Reset();
      return Slice();
    }
    case ChunkType::kMeta:
    {
      //Timer timer;timer.Start();
      QLBTreeMeta meta(node);
      //std::cerr << "meta " << meta.numEntries() << std::endl;
      auto index = meta.BinarySearch(key, 0, meta.numEntries());
      auto child_id = (index == meta.numEntries())? "_INFI_" :
          meta.GetKey(index).ToString();
      auto child_key = prefix_ + std::to_string(meta.GetLevel() - 1) + "|"
          + child_id;
      //Metrics::metascan += timer.ElapsedMilliseconds();timer.Reset();timer.Start();
      auto child = db_->Get(child_key);
      //Metrics::load += timer.ElapsedMilliseconds(); timer.Reset();
      return TryGet(child, key);
    }
    default:
      return Slice();
  }
}

std::map<std::string, std::string> QLBTree::Range(const Slice& start,
    const Slice& end) const {
  std::map<std::string, std::string> result;
  TryRange(root_node_->chunk(), start, end, result);
  return result;
}

void QLBTree::TryRange(const Chunk* node, const Slice& start,
    const Slice& end, std::map<std::string, std::string>& result) const {
  switch(node->type()) {
    case ChunkType::kMap:
    {
      FindKey(node, start, end, result);
      break;
    }
    case ChunkType::kMeta:
    {
      QLBTreeMeta meta(node);
      auto from = meta.BinarySearch(start, 0, meta.numEntries());
      auto to = meta.BinarySearch(end, 0, meta.numEntries());
      for (size_t i = from; i <= to; ++i) {
        auto child_id = (i == meta.numEntries())? "_INFI_" :
            meta.GetKey(i).ToString();
        auto child_key = prefix_ + std::to_string(meta.GetLevel() - 1) + "|"
            + child_id;
        //Metrics::metascan += timer.ElapsedMilliseconds();timer.Reset();timer.Start();
        auto child = db_->Get(child_key);
        //Metrics::load += timer.ElapsedMilliseconds(); timer.Reset();
        TryRange(child, start, end, result);
      }
      break;
    }
    default:
      std::cerr << "Wrong node type!" << std::endl;
  }
}

void QLBTree::FindKey(const Chunk* node, const Slice& from,
    const Slice& to, std::map<std::string, std::string>& result) const {
  auto num_entry = *reinterpret_cast<const uint32_t*>(node->data() +
      sizeof(uint64_t) + sizeof(uint32_t));
  size_t offset = sizeof(uint64_t) + sizeof(uint32_t) * 2;
  for (size_t i = 0; i < num_entry; ++i) {
    auto num_bytes = *reinterpret_cast<const uint32_t*>(node->data() + offset);
    offset += sizeof(uint32_t);

    auto key_bytes = *reinterpret_cast<const uint32_t*>(node->data() + offset);
    offset += sizeof(uint32_t);

    Slice key(node->data() + offset, key_bytes);
    offset += key_bytes;

    auto val_bytes = num_bytes - key_bytes - sizeof(uint32_t) * 2;
    if (!(key < from) && !(key > to)) {
      Slice val(node->data() + offset, val_bytes);
      result.emplace(key.ToString(), val.ToString());
    } else if (key > to) {
      break;
    }
    offset += val_bytes;
  }
}

bool QLBTree::Set(const std::vector<Slice>& keys,
    const std::vector<Slice>& vals) {
  if (keys.size() == 0) return false;

  Chunk new_root(root_node_->head());
  QLBTreeInsertResult retval;
  std::string root_id;
  for (size_t i = 0; i < keys.size(); ++i) {
    retval = Insert(&new_root, keys[i], vals[i]);
    if (retval.isSplit) {
      std::vector<Slice> new_keys;
      new_keys.emplace_back(retval.split_key);
      auto chunk = QLBTreeMeta::Encode(retval.level + 1, new_keys,
          retval.num_elems);
      root_id = prefix_ + std::to_string(retval.level + 1) + "|_INFI_";
      delta_->CreateChunk(root_id, std::move(chunk));
    } else {
      root_id = prefix_ + std::to_string(retval.level) + "|_INFI_";
    }
    new_root = delta_->GetChunk(root_id);
  }
  delta_->Commit(root_id, true, prefix_);
  delta_->Clear();
  auto root = db_->Get(prefix_ + "ROOT");
  // if (new_root.type() == ChunkType::kMeta) {
  //   root_node_.reset(new QLBTreeMeta(&new_root));
  // } else {
  //   root_node_.reset(new QLBTreeMap(&new_root));
  // }
  root_node_ = QLBTreeNode::CreateFromChunk(root);
  return true;
}
  
bool QLBTree::Set(const Slice& key, const Slice& val) {
  Chunk new_root(root_node_->head());
  auto retval = Insert(&new_root, key, val);
  std::string root_id;
  if (retval.isSplit) {
    new_root = QLBTreeMeta::Encode(retval.level, {Slice(retval.split_key)},
        retval.num_elems);
    root_id = prefix_ + std::to_string(retval.level + 1) + "|_INFI_";
    delta_->CreateChunk(root_id, Chunk(new_root.head()));
  } else {
    root_id = prefix_ + std::to_string(retval.level) + "|_INFI_";
  }
  delta_->Commit(root_id, true, prefix_);
  delta_->Clear();
  auto root = db_->Get(prefix_ + "ROOT");
  if (new_root.type() == ChunkType::kMeta) {
    root_node_.reset(new QLBTreeMeta(root));
  } else {
    root_node_.reset(new QLBTreeMap(root));
  }
  return true;
  return true;
}

QLBTreeInsertResult QLBTree::Insert(const Chunk* node, const Slice& key,
    const Slice& value) const {
  switch (node->type()) {
    case ChunkType::kMeta:
    {
      // search leaf node for update
      QLBTreeMeta meta(node);
      auto index = meta.BinarySearch(key, 0, meta.numEntries());
      auto child_id = index == meta.numEntries()? "_INFI_" :
          meta.GetKey(index).ToString();
      auto child_key = prefix_ + std::to_string(meta.GetLevel() - 1) + "|"
          + child_id;
      QLBTreeInsertResult retval;
      auto child = delta_->GetChunk(child_key);
      if (!child.empty()) {
        retval = Insert(&child, key, value);
      } else {
        auto child_ptr = db_->Get(child_key);
        retval = Insert(child_ptr, key, value);
      }
      auto result = meta.UpdateChild(index, retval, delta_.get(), prefix_);
      return std::move(result);
    }
    case ChunkType::kMap:
    {
      QLBTreeMap map_node(node);

      std::vector<Slice> newkeys;
      std::vector<Slice> newvals;
      size_t i = 0;
      bool inserted = false;
      while (i < map_node.numEntries()) {
        if (!inserted && key < map_node.GetKey(i)) {
          newkeys.emplace_back(key);
          newvals.emplace_back(value);
          inserted = true;
        }
        newkeys.emplace_back(map_node.GetKey(i));
        if (key == map_node.GetKey(i)) {
          newvals.emplace_back(value);
          inserted = true;
        } else {
          newvals.emplace_back(map_node.GetVal(i));
        }
        ++i;
      }
      if (!inserted) {
        newkeys.emplace_back(key);
        newvals.emplace_back(value);
      }

      QLBTreeInsertResult retval;
      if (newkeys.size() <= BPlusConfig::leaf_fanout()) {
        auto next = map_node.GetNext();
        Chunk new_root = QLBTreeMap::Encode(0, newkeys, newvals, next);
        auto node_id = prefix_ + "0|" + (next.empty()? "_INFI_" :
            newkeys[newkeys.size() - 1].ToString());
        retval = {false, "", 0, {newkeys.size()}};
        delta_->CreateChunk(node_id, std::move(new_root));
      } else {
        size_t split_idx = BPlusConfig::ComputeSplitIndex(newkeys.size());
        auto split_key = newkeys[split_idx - 1].ToString();
        std::vector<Slice> newkeys1(newkeys.begin(),
            newkeys.begin() + split_idx);
        std::vector<Slice> newkeys2(newkeys.begin() + split_idx,
            newkeys.end());
        std::vector<Slice> newvals1(newvals.begin(),
            newvals.begin() + split_idx);
        std::vector<Slice> newvals2(newvals.begin() + split_idx,
            newvals.end());

        auto node_id1 = prefix_ + "0|" + split_key;
        auto node_id2 = prefix_ + "0|" + (map_node.GetNext().empty()?
            "_INFI_" : newkeys2[newkeys2.size() - 1].ToString());
        Chunk new_root1 = QLBTreeMap::Encode(0, newkeys1, newvals1,
            Slice(node_id2));
        Chunk new_root2 = QLBTreeMap::Encode(0, newkeys2, newvals2,
            map_node.GetNext());
        retval = {true, split_key, 0, {newkeys1.size(), newkeys2.size()}};
        delta_->CreateChunk(node_id1, std::move(new_root1));
        delta_->CreateChunk(node_id2, std::move(new_root2));
      }
      return std::move(retval);
    }
    default:
      return QLBTreeInsertResult();
  }
}

void QLBTreeDelta::Commit(const std::string& id, bool isroot,
    const std::string& prefix) {
  auto node = GetChunk(id);
  if (node.empty()) {
    return;
  }
  if (isroot) {
    db_->Put(prefix + "ROOT", node);
  } else {
    db_->Put(id, node);
  }
  if (node.type() == ChunkType::kMeta) {
    // commit child
    QLBTreeMeta meta(&node);
    for (size_t i = 0; i < meta.numEntries(); ++i) {
      auto childid = prefix + std::to_string(meta.GetLevel() - 1) +
          "|" + meta.GetKey(i).ToString();
      Commit(childid, false, prefix);
    }
    if (meta.GetChildElems(meta.numEntries()) > 0) {
      auto childid = prefix + std::to_string(meta.GetLevel() - 1) +
          "|_INFI_";
      Commit(childid, false, prefix);
    }
  }
}

}  // namespace qldb

}  // namespace ledgebase
