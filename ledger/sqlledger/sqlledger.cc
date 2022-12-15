#include "ledger/sqlledger/sqlledger.h"
#include "ledger/common/utils.h"

namespace ledgebase {

namespace sqlledger {

SQLLedger::SQLLedger(int t, const std::string& dbpath) :
    logger_("/tmp/wal", "/tmp/index") {
  db_.Open(dbpath);
  mt_.reset(new MerkleTree(&db_));
  block_seq_ = 0;
  tid_ = 0;
  buffer_.reset(new std::map<uint64_t, std::vector<std::string>>());
  buildThread_.reset(new std::thread(&SQLLedger::updateLedger, this, t));
  qlstore_.reset(new qldb::QLDBStore());
  indexed_.reset(new qldb::QLBTree(qlstore_.get(), "COMMITTED_"));
  history_.reset(new qldb::QLBTree(qlstore_.get(), "HISTORY_"));
  dummy_ = Hash::ComputeFrom("0");
}

void SQLLedger::updateLedger(int timeout) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    if (buffer_->size() == 0) {
      continue;
    }

    timeval t0, t1;
    gettimeofday(&t0, NULL);
    std::unique_ptr<std::map<uint64_t, std::vector<std::string>>> new_txns;
    uint64_t new_blk_seq;
    {
      boost::unique_lock<boost::shared_mutex> blockseqlock(lock_);
      new_blk_seq = block_seq_ ++;
      new_txns = std::move(buffer_);
      buffer_.reset(new std::map<uint64_t, std::vector<std::string>>());
      logger_.index(new_blk_seq);
    }

    int txn_seq = 0;
    long nkey = 0;
    std::vector<std::string> leaves;
    for (auto& entry : *(new_txns.get())) {
      std::string txnid = "txn" + std::to_string(entry.first);
      nkey += entry.second.size();

      // build merkle txn merkle tree
      std::string level, txnroot;
      mt_->Build(txnid, entry.second, &level, &txnroot);

      // create transaction entries
      std::string txn_entry = txnid + "|" + std::to_string(new_blk_seq) + "|" + 
          std::to_string(txn_seq) + "|" + txnroot + "|" + level;
      db_.Put(txnid, txn_entry);

      leaves.emplace_back(txn_entry);
      txn_seq++;
    }

    std::string block_key = "blk" + std::to_string(new_blk_seq);
    // build merkle tree
    std::string level, root_hash;
    mt_->Build(block_key, leaves, &level, &root_hash);

    // get prev hash
    std::string digest;
    db_.Get("digest", &digest);
    auto prev_hash = Utils::splitBy(digest, '|')[0];

    // create new block
    std::string newblock = prev_hash + "|" + root_hash + "|" + level;
    std::string newhash = Hash::ComputeFrom(newblock).ToBase32();
    db_.Put(block_key, newblock);
    db_.Put("digest", newhash + "|" + std::to_string(new_blk_seq));

    gettimeofday(&t1, NULL);
    auto lat = (t1.tv_sec - t0.tv_sec)*1000000 + t1.tv_usec - t0.tv_usec;
    //std::cerr << "persist " << lat << " " << nkey << " " << new_txns->size() << std::endl;
  }
}

uint64_t SQLLedger::Set(const std::vector<std::string>& keys,
                        const std::vector<std::string>& vals) {
  // assign txn id
  std::string txnid = "txn" + std::to_string(tid_);

  uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  uint64_t next_blk_seq = 0;
  std::vector<std::string> docs;
  {
    boost::shared_lock<boost::shared_mutex> read(lock_);
    next_blk_seq = block_seq_;

    // create documents & update index
    for (size_t i = 0; i < keys.size(); ++i) {
      std::string document = std::to_string(next_blk_seq) + "|" + txnid + "|" +
                            std::to_string(i) + "|" + keys[i] + "|" +
                            vals[i] + "|" + std::to_string(now);
      docs.emplace_back(document);
    }
    buffer_->emplace(tid_, docs);
  }

  std::vector<Slice> ks, ds;
  for (size_t i = 0; i < keys.size(); ++i) {
    ks.emplace_back(keys[i]);
    ds.emplace_back(docs[i]);
    std::string hist_key = keys[i] + "|" + std::to_string(next_blk_seq);
    db_.Put(hist_key, docs[i]);
  }
  indexed_->Set(ks, ds);

  // append to wal log
  auto txn_chunk = TxnEntry::Encode(now, keys, vals, dummy_, tid_-1);
  logger_.log(txn_chunk.head(), txn_chunk.numBytes());

  {
    boost::unique_lock<boost::shared_mutex> exclusive(lock_);
    logger_.advance(txn_chunk.numBytes());
  }

  ++tid_;
  return next_blk_seq;
}

void SQLLedger::GetDigest(uint64_t* tip, std::string* hash) {
  std::string digest;
  db_.Get("digest", &digest);
  auto res = Utils::splitBy(digest, '|');
  *hash = res[0];
  *tip = std::stoul(res[1]);
}

std::string SQLLedger::GetCommitted(const std::string& key) const {
  auto result = indexed_->Get(Slice(key));
  return result.ToString();
}

std::string SQLLedger::GetDataAtBlock(const std::string& key,
                                      const uint64_t& block_seq) {
  // auto iter = db_.NewIterater();
  // for (iter->Seek(key + "|");
  //      iter->Valid() && iter->key().starts_with(key + "|");
  //      iter->Next()) {
  //   auto blk = std::stoul(Utils::splitBy(iter->key().ToString(), '|')[1]);
  //   if (blk == block_seq) {
  //     return iter->value().ToString();
  //   }
  // }
  auto histkey = key + "|" + std::to_string(block_seq);
  std::string value;
  db_.Get(histkey, &value);
  return value;
}

std::map<std::string, std::string> SQLLedger::Range(const std::string& from,
    const std::string& to) {
  auto result = indexed_->Range(Slice(from), Slice(to));
  return result;
}

std::vector<std::string> SQLLedger::GetHistory(const std::string& key,
    size_t n) {
  std::vector<std::string> retval;
  if (n == 0) return retval;

  auto iter = db_.NewIterater();
  int cnt = 0;
  for (iter->Seek(key + "|");
       iter->Valid() && iter->key().starts_with(key + "|");
       iter->Next()) {
    
    retval.emplace_back(iter->value().ToString());
    ++cnt;
    if (cnt >= n) break;
  }
  return retval;
}

SQLLedgerProof SQLLedger::getProof(const std::string& key,
    const uint64_t block_addr) {
  SQLLedgerProof proof;

  // get digest
  std::string value;
  db_.Get("digest", &value);
  auto digest = Utils::splitBy(value, '|');
  proof.digest = digest[0];
  int tip = std::stoul(digest[1]);

  // get block
  int level;
  std::string blkroot;
  //std::cout << "blocks " << (tip - block_addr) << std::endl;
  for (size_t i = block_addr; i <= tip; ++i) {
    std::string block;
    db_.Get("blk" + std::to_string(i), &block);
    proof.blks.emplace_back(block);
    if (i == block_addr) {
      auto blockinfo = Utils::splitBy(block, '|');
      level = std::stoul(blockinfo[2]);
      blkroot = blockinfo[1];
    }
  }

  std::string doc = GetDataAtBlock(key, block_addr);
  auto docitems = Utils::splitBy(doc, '|');
  auto txnid = docitems[1];
  auto docseq = std::stoul(docitems[2]);

  std::string txn;
  db_.Get(txnid, &txn);
  auto txnitems = Utils::splitBy(txn, '|');
  auto txnseq = std::stoul(txnitems[2]);
  auto txnroot = txnitems[3];
  auto txnrootlevel = std::stoul(txnitems[4]);

  proof.blk_proof = mt_->GetProof("blk" + std::to_string(block_addr),
      level, txnseq);
  proof.blk_proof.value = txn;
  proof.txn_proof = mt_->GetProof("txn" + txnid, txnrootlevel, docseq);
  proof.txn_proof.value = doc;
  //std::cout << "height " << level + txnrootlevel << std::endl;
  return proof;
}

bool SQLLedgerProof::Verify() {
  std::string target = digest;
  std::string blk_mt_root;
  int blocks = 0;
  for (auto it = blks.rbegin(); it != blks.rend(); ++it) {
    if (Hash::ComputeFrom(*it).ToBase32().compare(target) != 0) {
      //std::cout << "verification failed at block" << std::endl;
    }
    auto block_info = Utils::splitBy(*it, '|');
    target = block_info[0];
    blk_mt_root = block_info[1];
    blocks++;
  }
  //std::cout << "block " << blks.size() << " " << blocks << std::endl;

  if (blk_mt_root.compare(blk_proof.digest) != 0) {
    //std::cout << "verification failed at txn mt root" << std::endl;
  }
  std::cout << "txn ";
  if (!blk_proof.Verify()) {
    //std::cout << "verification failed at txn tree" << std::endl;
  }

  auto txn_info = Utils::splitBy(blk_proof.value, '|');
  std::string txn_mt_root = txn_info[3];
  if (txn_mt_root != txn_proof.digest) {
    //std::cout << "verification failed at row mt root" << std::endl;
  }
  //std::cout << "row ";
  return txn_proof.Verify();
}

Auditor SQLLedger::getAudit(const uint64_t& seq) {
  Auditor auditor;
  uint64_t block_addr;
  auditor.txns = logger_.read(seq, &block_addr);
  auditor.block_seq = block_addr;

  // get digest
  std::string value;
  db_.Get("digest", &value);
  auto digest = Utils::splitBy(value, '|');
  auditor.digest = digest[0];
  int tip = std::stoul(digest[1]);

  // get block
  int level;
  std::string blkroot;
  for (size_t i = block_addr; i <= tip; ++i) {
    std::string block;
    db_.Get("blk" + std::to_string(i), &block);
    auditor.blks.emplace_back(block);
    if (i == block_addr) {
      auto blockinfo = Utils::splitBy(block, '|');
      level = std::stoul(blockinfo[2]);
      blkroot = blockinfo[1];
    }
  }
  return auditor;
}

bool Auditor::Audit(DB* db, size_t* ntxn) {
  MerkleTree mt(db);
  std::string target = digest;
  std::string blk_mt_root;
  for (auto it = blks.rbegin(); it != blks.rend(); ++it) {
    if (Hash::ComputeFrom(*it).ToBase32().compare(target) != 0) return false;
    auto block_info = Utils::splitBy(*it, '|');
    target = block_info[0];
    blk_mt_root = block_info[1];
  }

  auto entries = TxnEntry::ParseTxnEntries(txns);
  *ntxn = entries.size();
  std::vector<std::string> buffer;
  for (size_t j = 0; j < entries.size(); ++j) {
    auto entry = entries[j];
    std::string txnid = "txn" + std::to_string(entry.txnid());
    std::vector<std::string> documents;
    for (size_t i = 0; i < entry.keysize(); ++i) {
      std::string document = std::to_string(block_seq) + "|" + txnid + "|" +
          std::to_string(i) + "|" + entry.key(i).ToString() + "|" +
          entry.val(i).ToString() + "|" + std::to_string(entry.time());
      documents.emplace_back(document);
    }
    // build merkle txn merkle tree
    std::string level, txnroot;
    mt.Build(txnid, documents, &level, &txnroot);

    // create transaction entries
    std::string txn_entry = txnid + "|" + std::to_string(block_seq) + "|" + 
        std::to_string(j) + "|" + txnroot + "|" + level;

    buffer.emplace_back(txn_entry);
  }

  std::string block_key = "blk" + std::to_string(block_seq);
  std::string level, root_hash;
  mt.Build(block_key, buffer, &level, &root_hash);

  return root_hash.compare(blk_mt_root) == 0;
}

}  // namespace sqlledger

}  // namespace ledgebase
