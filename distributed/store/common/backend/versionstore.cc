#include "store/common/backend/versionstore.h"
#include <sys/time.h>

#include "ledger/common/utils.h"

using namespace std;

VersionedKVStore::VersionedKVStore() { }

VersionedKVStore::VersionedKVStore(const string& db_path, int timeout) {
#ifdef LEDGERDB
  ldb.reset(new ledgebase::ledgerdb::LedgerDB(timeout));
#endif
#ifdef AMZQLDB
  ledgebase::qldb::BPlusConfig::Init(45, 15);
  qldb_.reset(new ledgebase::qldb::QLDB());
#endif
}
    
VersionedKVStore::~VersionedKVStore() { }

bool VersionedKVStore::GetDigest(strongstore::proto::Reply* reply) {
  uint64_t tip;
  std::string hash;

#ifdef LEDGERDB
  ldb->GetRootDigest(&tip, &hash);
#endif

  auto digest = reply->mutable_digest();
  digest->set_block(tip);
  digest->set_hash(hash);

  return true;
}

bool VersionedKVStore::GetNVersions(
    std::vector<std::pair<std::string, size_t>>& ver_keys,
    strongstore::proto::Reply* reply) {
#ifdef LEDGERDB
  std::vector<std::vector<
      std::pair<uint64_t, std::pair<size_t, std::string>>>> get_val_res;
  std::vector<std::string> keys;
  for (auto& k : ver_keys) {
    keys.emplace_back(k.first);
  }
  size_t i = 0;
  ldb->GetVersions(keys, get_val_res, ver_keys[0].second);
  for (auto& kres : get_val_res) {
    for (auto& res : kres) {
      auto kv = reply->add_values();
      kv->set_key(keys[i]);
      kv->set_val(res.second.second);
      kv->set_estimate_block(res.second.first);
    }
    ++i;
  }
#endif
#ifdef AMZQLDB
  auto digestInfo = qldb_->digest("test");
  for (auto& key : ver_keys) {
    auto result = qldb_->GetHistory("test", key.first, key.second);
    for (auto& res : result) {
      ledgebase::qldb::Document doc(&res);
      auto proofres = qldb_->getProof("test", digestInfo.tip,
          doc.getAddr().seq_no, doc.getMetaData().doc_seq);
      auto p = reply->add_qproof();
      p->set_key(key.first);
      p->set_value(proofres.data.val.ToString());
      p->set_blockno(proofres.addr.seq_no);
      p->set_doc_seq(proofres.meta.doc_seq);
      p->set_version(proofres.meta.version);
      p->set_time(proofres.meta.time);
      for (auto& hash : proofres.proof) {
        p->add_hashes(hash);
      }
      for (auto& pos : proofres.pos) {
        p->add_pos(pos);
      }
    }
  }
#endif

  return true;
}

bool VersionedKVStore::BatchGet(const std::vector<std::string>& keys,
    strongstore::proto::Reply* reply) {
#ifdef LEDGERDB
  std::vector<std::pair<uint64_t, std::pair<size_t, std::string>>> get_val_res;
  ldb->GetValues(keys, get_val_res);
  size_t i = 0;
  for (auto& res : get_val_res) {
    auto kv = reply->add_values();
    kv->set_key(keys[i]);
    kv->set_val(res.second.second);
    kv->set_estimate_block(res.second.first);
    reply->add_timestamps(res.first);
    ++i;
  }
#endif
#ifdef AMZQLDB
  auto digest = reply->mutable_digest();
  auto digestInfo = qldb_->digest("test");
  digest->set_block(digestInfo.tip);
  digest->set_hash(digestInfo.digest);
  for (auto& key : keys) {
    auto result = qldb_->GetCommitted("test", key);
    if (result.empty()) {
      std::cout << key << " is empty" << std::endl;
      continue;
    }
    ledgebase::qldb::Document doc(&result);
    auto proofres = qldb_->getProof("test", digestInfo.tip, doc.getAddr().seq_no,
        doc.getMetaData().doc_seq);
    auto p = reply->add_qproof();
    p->set_key(key);
    p->set_value(proofres.data.val.ToString());
    p->set_blockno(proofres.addr.seq_no);
    p->set_doc_seq(proofres.meta.doc_seq);
    p->set_version(proofres.meta.version);
    p->set_time(proofres.meta.time);
    for (auto& hash : proofres.proof) {
      p->add_hashes(hash);
    }
    for (auto& pos : proofres.pos) {
      p->add_pos(pos);
    }
  }
#endif

  return true;
}

bool VersionedKVStore::GetRange(const std::string &start,
    const std::string &end, strongstore::proto::Reply* reply) {
#ifdef LEDGERDB
  std::map<std::string,
      std::pair<uint64_t, std::pair<size_t, std::string>>> range_res;
  ldb->GetRange(start, end, range_res);
  for (auto& res : range_res) {
    auto kv = reply->add_values();
    kv->set_key(res.first);
    kv->set_val(res.second.second.second);
    kv->set_estimate_block(res.second.second.first);
    reply->add_timestamps(res.second.first);
  }
#endif
#ifdef AMZQLDB
  auto digest = reply->mutable_digest();
  auto digestInfo = qldb_->digest("test");
  digest->set_block(digestInfo.tip);
  digest->set_hash(digestInfo.digest);
  auto results = qldb_->Range("test", start, end);
  for (auto& r : results) {
    ledgebase::Slice valslice(r.second);
    ledgebase::Chunk valchunk(valslice.data());
    ledgebase::qldb::Document doc(&valchunk);
    auto proofres = qldb_->getProof("test", digestInfo.tip, doc.getAddr().seq_no,
        doc.getMetaData().doc_seq);
    auto p = reply->add_qproof();
    p->set_key(r.first);
    p->set_value(proofres.data.val.ToString());
    p->set_blockno(proofres.addr.seq_no);
    p->set_doc_seq(proofres.meta.doc_seq);
    p->set_version(proofres.meta.version);
    p->set_time(proofres.meta.time);
    for (auto& hash : proofres.proof) {
      p->add_hashes(hash);
    }
    for (auto& pos : proofres.pos) {
      p->add_pos(pos);
    }
  }
#endif
  return true;
}

bool VersionedKVStore::GetProof(
    const std::map<uint64_t, std::vector<std::string>>& keys,
    strongstore::proto::Reply* reply) {
  timeval t0, t1;
  gettimeofday(&t0, NULL);
#ifdef LEDGERDB
  std::vector<ledgebase::ledgerdb::Proof> mtproof;
  std::vector<ledgebase::ledgerdb::MPTProof> mptproof;
  std::vector<std::string> blk_datas;
  std::string mtdigest;
  std::string mptdigest;
  size_t block;

  std::vector<std::string> ks;
  std::vector<uint64_t> blks;
  for (auto& vkey : keys) {
    for (auto& k : vkey.second) {
      ks.push_back(k);
      blks.push_back(vkey.first);
    }
  }
  
  ldb->GetProofs(ks, blks, mtproof, mptproof, &mtdigest,
      &block, &mptdigest);
  auto digest = reply->mutable_digest();
  digest->set_block(block);
  digest->set_hash(mtdigest);
  digest->set_mpthash(mptdigest);
  
  for (size_t i = 0; i < mtproof.size(); ++i) {
    auto p = reply->add_proof();
    p->set_val(mtproof[i].value);
    p->set_hash(mtproof[i].digest);
    for (size_t j = 0; j < mtproof[i].proof.size(); ++j) {
      p->add_proof(mtproof[i].proof[j]);
      p->add_mt_pos(mtproof[i].pos[j]);
    }
    p->set_mptvalue(mptproof[i].GetValue());
    for (size_t j = 0; j < mptproof[i].MapSize(); ++j) {
      p->add_mpt_chunks(mptproof[i].GetMapChunk(j));
      p->add_mpt_pos(mptproof[i].GetMapPos(j));
    }
  }

#endif
  gettimeofday(&t1, NULL);
  auto lat = (t1.tv_sec - t0.tv_sec)*1000000 + t1.tv_usec - t0.tv_usec;
  //std::cout << "getproof " << lat << " " << ks.size() << std::endl;
  return true;
}

bool VersionedKVStore::GetProof(const uint64_t& seq,
    strongstore::proto::Reply* reply) {
#ifdef LEDGERDB
  auto auditor = ldb->GetAudit(seq);
  auto reply_auditor = reply->mutable_laudit();
  reply_auditor->set_digest(auditor.digest);
  reply_auditor->set_commit_seq(auditor.commit_seq);
  reply_auditor->set_first_block_seq(auditor.first_block_seq);
  for (size_t i = 0; i < auditor.commits.size(); ++i) {
    reply_auditor->add_commits(auditor.commits[i]);
  }
  for (size_t i = 0; i < auditor.blocks.size(); ++i) {
    reply_auditor->add_blocks(auditor.blocks[i]);
  }
  for (auto& mptproof : auditor.mptproofs) {
    auto reply_mptproof = reply_auditor->add_mptproofs();
    reply_mptproof->set_value(mptproof.GetValue());
    for (size_t i = 0; i < mptproof.MapSize(); ++i) {
      reply_mptproof->add_chunks(mptproof.GetMapChunk(i));
      reply_mptproof->add_pos(mptproof.GetMapPos(i));
    }
  }
#endif
  return true;
}


void VersionedKVStore::put(const vector<string> &keys,
    const vector<string> &values, const Timestamp &t,
    strongstore::proto::Reply* reply)
{
#ifdef LEDGERDB
  auto estimate_blocks = ldb->Set(keys, values, t.getTimestamp());
  if (reply != nullptr) {
    auto kv = reply->add_values();
    for (size_t i = 0; i < keys.size(); ++i) {
      kv->set_key(keys[i]);
      kv->set_val(values[i]);
      kv->set_estimate_block(estimate_blocks);
    }
  }
#endif
#ifdef AMZQLDB
  qldb_->Set("test", keys, values);
  if (reply != nullptr) {
    BatchGet(keys, reply);
  }
#endif
}

bool VersionedKVStore::get(const std::string &key,
                           const Timestamp &t,
                           std::pair<Timestamp, std::string> &value)
{
#ifdef LEDGERDB
    std::vector<std::pair<uint64_t, std::pair<size_t, std::string>>> get_val_res;
    ldb->GetValues({key}, get_val_res);
    value = std::make_pair(get_val_res[0].second.first,
        get_val_res[0].second.second);
#endif
#ifdef AMZQLDB
    auto result = qldb_->GetCommitted("test", key);
    ledgebase::qldb::Document doc(&result);
    value = std::make_pair(Timestamp(doc.getMetaData().time),
        doc.getData().val.ToString());
#endif
    return true;
}
