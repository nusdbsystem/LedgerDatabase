#include "distributed/store/strongstore/shardclient.h"
#include <sys/time.h>

namespace strongstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(Mode mode, const string &configPath,
             Transport *transport, uint64_t client_id, int
             shard, int closestReplica)
  : transport(transport), client_id(client_id), shard(shard)
{
  ifstream configStream(configPath);
  if (configStream.fail()) {
    fprintf(stderr, "unable to read configuration file: %s\n",
        configPath.c_str());
  }
  transport::Configuration config(configStream);

  client = new replication::vr::VRClient(config, transport);

  if (mode == MODE_OCC || mode == MODE_SPAN_OCC) {
    if (closestReplica == -1) {
      replica = client_id % config.n;
    } else {
      replica = closestReplica;
    }
      } else {
    replica = 0;
  }

  waiting = NULL;
  blockingBegin = NULL;
  uid = 0;
  tip_block = 0;
  audit_block = -1;
  auto status = db_.Open("/tmp/auditor"+std::to_string(shard)+".store");
  if (!status) std::cout << "auditor db open failed" << std::endl;
}

ShardClient::~ShardClient()
{
  delete client;
}

/* Sends BEGIN to a single shard indexed by i. */
void
ShardClient::Begin(uint64_t id)
{

  // Wait for any previous pending requests.
  if (blockingBegin != NULL) {
    blockingBegin->GetReply();
    delete blockingBegin;
    blockingBegin = NULL;
  }
}

void
ShardClient::BatchGet(uint64_t id, const std::vector<std::string> &keys,
    Promise *promise) {
  // create prepare request
  string request_str;
  Request request;
  request.set_op(Request::BATCH_GET);
  request.set_txnid(id);
  auto batchget = request.mutable_batchget();
  for (auto k : keys) {
    batchget->add_keys(k);
  }
  request.SerializeToString(&request_str);

  int timeout = 100000;
  transport->Timer(0, [=]() {
    waiting = promise;
    client->InvokeUnlogged(replica,
                           request_str,
                           bind(&ShardClient::BatchGetCallback,
                            this,
                            placeholders::_1,
                            placeholders::_2),
                           bind(&ShardClient::GetTimeout,
                            this),
                           timeout);
  });
}

void
ShardClient::GetRange(uint64_t id, const std::string& from,
                      const std::string& to, Promise* promise) {
  // create request
  string request_str;
  Request request;
  request.set_op(Request::RANGE);
  request.set_txnid(id);
  auto range = request.mutable_range();
  range->set_from(from);
  range->set_to(to);
  request.SerializeToString(&request_str);

  // set to 1 second by default
  int timeout = 100000;
  transport->Timer(0, [=]() {
    waiting = promise;
    client->InvokeUnlogged(replica,
                 request_str,
                 bind(&ShardClient::GetRangeCallback,
                  this,
                  placeholders::_1,
                  placeholders::_2),
                 bind(&ShardClient::GetTimeout,
                  this),
                 timeout); // timeout in ms
  });
}

bool
ShardClient::GetProof(const uint64_t block,
                      const std::vector<string>& keys,
                      Promise* promise) {
  if (tip_block < block) {
    promise->Reply(REPLY_OK);
    return false;
  }
  // create request
  string request_str;
  Request request;
  request.set_op(Request::VERIFY_GET);
  request.set_txnid(0);
  auto verify = request.mutable_verify();
  verify->set_block(block);
  for (auto& k : keys) {
    verify->add_keys(k);
  }
  request.SerializeToString(&request_str);

  // set to 1 second by default
  int timeout = 100000;
  transport->Timer(0, [=]() {
    verifyPromise.emplace(uid, promise);
    ++uid;
    client->InvokeUnlogged(replica,
                 request_str,
                 bind(&ShardClient::GetProofCallback,
                  this,
                  uid - 1,
                  keys,
                  placeholders::_1,
                  placeholders::_2),
                 bind(&ShardClient::GetTimeout,
                  this),
                 timeout); // timeout in ms
  });
  return true;
}

bool
ShardClient::Audit(const uint64_t& seq, Promise* promise) {
  // create request
  string request_str;
  Request request;
  request.set_op(Request::AUDIT);
  request.set_txnid(0);
  auto verify = request.mutable_audit();
  verify->set_seq(seq);
  request.SerializeToString(&request_str);

  // set to 1 second by default
  int timeout = 100000;
  transport->Timer(0, [=]() {
    verifyPromise.emplace(uid, promise);
    ++uid;
    client->InvokeUnlogged(replica,
                 request_str,
                 bind(&ShardClient::AuditCallback,
                  this,
                  seq,
                  uid - 1,
                  placeholders::_1,
                  placeholders::_2),
                 bind(&ShardClient::GetTimeout,
                  this),
                 timeout); // timeout in ms
  });
  return true;
}

void
ShardClient::Prepare(uint64_t id, const Transaction &txn,
          const Timestamp &timestamp, Promise *promise)
{

  // create prepare request
  string request_str;
  Request request;
  request.set_op(Request::PREPARE);
  request.set_txnid(id);
  txn.serialize(request.mutable_prepare()->mutable_txn());
  request.SerializeToString(&request_str);

  timeval t;
  gettimeofday(&t, NULL);
  transport->Timer(0, [=]() {
	  waiting = promise;
      client->Invoke(request_str,
               bind(&ShardClient::PrepareCallback,
                this,
                placeholders::_1,
                placeholders::_2));
    });
}

void
ShardClient::Commit(uint64_t id, const Transaction &txn,
    const std::vector<std::pair<std::string, size_t>>& versionedKeys,
    uint64_t timestamp, Promise *promise)
{
  // create commit request
  string request_str;
  Request request;
  request.set_op(Request::COMMIT);
  request.set_txnid(id);
  request.mutable_commit()->set_timestamp(timestamp);
  if (versionedKeys.size() > 0) {
    auto ver_msg = request.mutable_version();
    for (auto& vk : versionedKeys) {
      auto ver_keys = ver_msg->add_versionedkeys();
      ver_keys->set_key(vk.first);
      ver_keys->set_nversions(vk.second);
    }
  }
  request.SerializeToString(&request_str);

  blockingBegin = new Promise(COMMIT_TIMEOUT);
  timeval t;
  gettimeofday(&t, NULL);
  transport->Timer(0, [=]() {
    waiting = promise;

    client->Invoke(request_str,
      bind(&ShardClient::CommitCallback,
        this,
        placeholders::_1,
        placeholders::_2));
  });
}

/* Aborts the ongoing transaction. */
void
ShardClient::Abort(uint64_t id, const Transaction &txn, Promise *promise)
{

  // create abort request
  string request_str;
  Request request;
  request.set_op(Request::ABORT);
  request.set_txnid(id);
  txn.serialize(request.mutable_abort()->mutable_txn());
  request.SerializeToString(&request_str);

  blockingBegin = new Promise(ABORT_TIMEOUT);
  transport->Timer(0, [=]() {
	  waiting = promise;

	  client->Invoke(request_str,
			   bind(&ShardClient::AbortCallback,
				this,
				placeholders::_1,
				placeholders::_2));
  });
}

void
ShardClient::AuditCallback(uint64_t seq, size_t uid,
                           const std::string& request_str,
                           const std::string& reply_str) {
  /* Replies back from a shard. */
  Reply reply;
  reply.ParseFromString(reply_str);
  if (verifyPromise[uid] != NULL) {
    tip_block = reply.digest().block();
    VerifyStatus res;
    if (seq <= tip_block) {
      size_t nblocks = 0, ntxns = 0;
#ifdef SQLLEDGER
      ledgebase::sqlledger::Auditor auditor;
      auto reply_audit = reply.saudit();

      nblocks = reply_audit.block_no() - audit_block;
      audit_block = reply_audit.block_no();
      auditor.digest = reply_audit.digest();
      auditor.block_seq = reply_audit.block_no();
      auditor.txns = reply_audit.txns();
      for (int i = 0; i < reply_audit.blocks_size(); ++i) {
        auditor.blks.emplace_back(reply_audit.blocks(i));
      }
      if (auditor.Audit(&db_, &ntxns)) {
        res = VerifyStatus::PASS;
      } else {
        res = VerifyStatus::FAILED;
      }
#endif
#ifdef LEDGERDB
      ledgebase::ledgerdb::Auditor auditor;
      auto reply_audit = reply.laudit();
      nblocks = reply_audit.blocks_size();
      ntxns = nblocks;
      auditor.digest = reply_audit.digest();
      auditor.commit_seq = reply_audit.commit_seq();
      auditor.first_block_seq = reply_audit.first_block_seq();
      for (int i = 0; i < reply_audit.commits_size(); ++i) {
        auditor.commits.emplace_back(reply_audit.commits(i));
      }
      for (int i = 0; i < reply_audit.blocks_size(); ++i) {
        auditor.blocks.emplace_back(reply_audit.blocks(i));
      }
      for (int i = 0; i < reply_audit.mptproofs_size(); ++i) {
        auto p = reply_audit.mptproofs(i);
        ledgebase::ledgerdb::MPTProof mptproof;
        mptproof.SetValue(p.value());
        for (int j = 0; j < p.chunks_size(); ++j) {
          mptproof.AppendProof(
              reinterpret_cast<const unsigned char*>(p.chunks(j).c_str()),
              p.pos(j));
        }
        auditor.mptproofs.emplace_back(mptproof);
      }
      if (auditor.Audit(&db_)) {
        res = VerifyStatus::PASS;
      } else {
        res = VerifyStatus::FAILED;
      }
#endif

      std::cout << "# " << nblocks << " " << ntxns << std::endl;
    }

    Promise *w = verifyPromise[uid];
    verifyPromise.erase(uid);
    w->Reply(reply.status(), res);
  }
}

void
ShardClient::GetProofCallback(size_t uid,
                              const std::vector<std::string>& keys,
                              const std::string& request_str,
                              const std::string& reply_str) {
  /* Replies back from a shard. */
  Reply reply;
  reply.ParseFromString(reply_str);
  if (verifyPromise[uid] != NULL) {
    tip_block = reply.digest().block();
    VerifyStatus res = VerifyStatus::PASS;

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

#ifdef LEDGERDB
    ledgebase::Hash mptdigest = ledgebase::Hash::FromBase32(reply.digest().mpthash());
    for (size_t i = 0; i < reply.proof_size(); ++i) {
      auto p = reply.proof(i);
      ledgebase::ledgerdb::MPTProof prover;
      prover.SetValue(p.mptvalue());
      for (size_t j = 0; j < p.mpt_chunks_size(); ++j) {
        prover.AppendProof(
            reinterpret_cast<const unsigned char*>(p.mpt_chunks(j).c_str()),
            p.mpt_pos(j));
      }
      if (p.mpt_chunks_size() > 0 && !prover.VerifyProof(mptdigest, keys[i])) {
        res = VerifyStatus::FAILED;
      }

      ledgebase::ledgerdb::Proof mtprover;
      mtprover.value = p.val();
      mtprover.digest = p.hash();
      for (size_t j = 0; j < p.proof_size(); ++j) {
        mtprover.proof.emplace_back(p.proof(j));
      }
      for (size_t j = 0; j < p.mt_pos_size(); ++j) {
        mtprover.pos.emplace_back(p.mt_pos(j));
      }
      if (!mtprover.Verify()) {
        res = VerifyStatus::FAILED;
      }
    }
#endif
#ifdef SQLLEDGER
    res = VerifyStatus::PASS;
    std::string ledger = "test";
    auto digest = ledgebase::Hash::FromBase32(reply.digest().hash());
    for (int i = 0; i < reply.sproof_size(); ++i) {
      ledgebase::sqlledger::SQLLedgerProof prover;
      auto curr_proof = reply.sproof(i);
      prover.digest = curr_proof.digest();
      for (int j = 0; j < curr_proof.blocks_size(); ++j) {
        prover.blks.emplace_back(curr_proof.blocks(j));
      }
      auto blk_proof = curr_proof.blk_proof();
      prover.blk_proof.digest = blk_proof.digest();
      prover.blk_proof.value  = blk_proof.value();
      for (int j = 0; j < blk_proof.proof_size(); ++j) {
        prover.blk_proof.proof.emplace_back(blk_proof.proof(j));
        prover.blk_proof.pos.emplace_back(blk_proof.pos(j));
      }

      auto txn_proof = curr_proof.txn_proof();
      prover.txn_proof.digest = txn_proof.digest();
      prover.txn_proof.value  = txn_proof.value();
      for (int j = 0; j < txn_proof.proof_size(); ++j) {
        prover.txn_proof.proof.emplace_back(txn_proof.proof(j));
        prover.txn_proof.pos.emplace_back(txn_proof.pos(j));
      }
      
      if (!prover.Verify()) {
        res = VerifyStatus::FAILED;
      }
    }
#endif

    gettimeofday(&t1, NULL);
    auto elapsed = ((t1.tv_sec - t0.tv_sec)*1000000 +
                    (t1.tv_usec - t0.tv_usec));
    //std::cout << "verify " << elapsed << " " << reply.ByteSizeLong() << " " << keys.size() << " " << res << std::endl;

    Promise *w = verifyPromise[uid];
    verifyPromise.erase(uid);
    w->Reply(reply.status(), res);
  }
}

void
ShardClient::GetRangeCallback(const string &request_str, const string &reply_str)
{
  Reply reply;
  reply.ParseFromString(reply_str);
  if (waiting != NULL) {
    std::vector<Timestamp> timestamps;
    std::map<std::string, std::string> values;
    std::vector<std::string> unverified_keys;
    std::vector<uint64_t> estimate_blocks;

#if defined(LEDGERDB) || defined(SQLLEDGER)
    tip_block = reply.digest().block();
    for (size_t i = 0; i < reply.values_size(); ++i) {
      auto v = reply.values(i);
      unverified_keys.emplace_back(v.key());
      estimate_blocks.push_back(v.estimate_block());
      values.emplace(v.key(), v.val());
    }
    for (size_t i = 0; i < reply.timestamps_size(); ++i) {
      timestamps.emplace_back(reply.timestamps(i));
    }
#endif
#ifdef AMZQLDB
    VerifyStatus vs = VerifyStatus::PASS;
    ledgebase::Hash digest;
    std::string ledger = "test";
    digest = ledgebase::Hash::FromBase32(reply.digest().hash());
    for (int i = 0; i < reply.qproof_size(); ++i) {
      ledgebase::qldb::QLProofResult prover;
      auto curr_proof = reply.qproof(i);
      prover.addr.ledger_name = ledgebase::Slice(ledger);
      prover.addr.seq_no = curr_proof.blockno();
      prover.data.key = ledgebase::Slice(curr_proof.key());
      prover.data.val = ledgebase::Slice(curr_proof.value());
      prover.meta.doc_seq = curr_proof.doc_seq();
      prover.meta.version = curr_proof.version();
      prover.meta.time = curr_proof.time();
      for (int j = 0; j < curr_proof.hashes_size(); ++j) {
        prover.proof.emplace_back(curr_proof.hashes(j));
      }
      for (int j = 0; j < curr_proof.pos_size(); ++j) {
        prover.pos.push_back(curr_proof.pos(j));
      }

      values.emplace(curr_proof.key(), curr_proof.value());
      timestamps.emplace_back(curr_proof.time());

      if (!prover.Verify(digest)) {
        vs = VerifyStatus::FAILED;
      }
    }
#endif

    Promise *w = waiting;
    waiting = NULL;
    w->Reply(reply.status(), timestamps, values, unverified_keys, estimate_blocks);
  }
}

void
ShardClient::BatchGetCallback(const string &request_str, const string &reply_str)
{
  Reply reply;
  reply.ParseFromString(reply_str);
  if (waiting != NULL) {
    std::vector<Timestamp> timestamps;
    std::map<std::string, std::string> values;
    std::vector<uint64_t> estimate_blocks;
    std::vector<std::string> unverified_keys;

#if defined(LEDGERDB) || defined(SQLLEDGER)
    tip_block = reply.digest().block();
    for (size_t i = 0; i < reply.values_size(); ++i) {
      auto v = reply.values(i);
      unverified_keys.emplace_back(v.key());
      estimate_blocks.push_back(v.estimate_block());
      values.emplace(v.key(), v.val());
    }
    for (size_t i = 0; i < reply.timestamps_size(); ++i) {
      timestamps.emplace_back(reply.timestamps(i));
    }
#endif
#ifdef AMZQLDB
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    VerifyStatus vs = VerifyStatus::PASS;
    ledgebase::Hash digest;
    std::string ledger = "test";
    digest = ledgebase::Hash::FromBase32(reply.digest().hash());
    for (int i = 0; i < reply.qproof_size(); ++i) {
      ledgebase::qldb::QLProofResult prover;
      auto curr_proof = reply.qproof(i);
      prover.addr.ledger_name = ledgebase::Slice(ledger);
      prover.addr.seq_no = curr_proof.blockno();
      prover.data.key = ledgebase::Slice(curr_proof.key());
      prover.data.val = ledgebase::Slice(curr_proof.value());
      prover.meta.doc_seq = curr_proof.doc_seq();
      prover.meta.version = curr_proof.version();
      prover.meta.time = curr_proof.time();
      for (int j = 0; j < curr_proof.hashes_size(); ++j) {
        prover.proof.emplace_back(curr_proof.hashes(j));
      }
      for (int j = 0; j < curr_proof.pos_size(); ++j) {
        prover.pos.push_back(curr_proof.pos(j));
      }

      values.emplace(curr_proof.key(), curr_proof.value());
      timestamps.emplace_back(curr_proof.time());

      if (!prover.Verify(digest)) {
        vs = VerifyStatus::FAILED;
      }
    }
    gettimeofday(&t1, NULL);
    auto elapsed = ((t1.tv_sec - t0.tv_sec)*1000000 +
                    (t1.tv_usec - t0.tv_usec));
    // std::cout << "verify " << elapsed << " " << reply.ByteSizeLong() << " " << reply.qproof_size() << " " << vs << std::endl;
#endif

    Promise *w = waiting;
    waiting = NULL;
    w->Reply(reply.status(), timestamps, values, unverified_keys, estimate_blocks);
  }
}

void
ShardClient::CommitCallback(const string &request_str, const string &reply_str)
{
  Reply reply;
  reply.ParseFromString(reply_str);
  ASSERT(reply.status() == REPLY_OK);

  ASSERT(blockingBegin != NULL);
  blockingBegin->Reply(0);

  if (waiting != NULL) {
    std::vector<uint64_t> estimate_blocks;
    std::vector<std::string> unverified_keys;
    VerifyStatus vs;
#if defined(LEDGERDB) || defined(SQLLEDGER)
    vs = VerifyStatus::UNVERIFIED;
    tip_block = reply.digest().block();
    for (size_t i = 0; i < reply.values_size(); ++i) {
      auto values = reply.values(i);
      unverified_keys.emplace_back(values.key());
      estimate_blocks.push_back(values.estimate_block());
    }
#endif
#ifdef AMZQLDB
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    vs = VerifyStatus::PASS;
    std::string ledger = "test";
    auto digest = ledgebase::Hash::FromBase32(reply.digest().hash());
    for (int i = 0; i < reply.qproof_size(); ++i) {
      ledgebase::qldb::QLProofResult prover;
      auto curr_proof = reply.qproof(i);
      prover.addr.ledger_name = ledgebase::Slice(ledger);
      prover.addr.seq_no = curr_proof.blockno();
      prover.data.key = ledgebase::Slice(curr_proof.key());
      prover.data.val = ledgebase::Slice(curr_proof.value());
      prover.meta.doc_seq = curr_proof.doc_seq();
      prover.meta.version = curr_proof.version();
      prover.meta.time = curr_proof.time();
      for (int j = 0; j < curr_proof.hashes_size(); ++j) {
        prover.proof.emplace_back(curr_proof.hashes(j));
      }
      for (int j = 0; j < curr_proof.pos_size(); ++j) {
        prover.pos.push_back(curr_proof.pos(j));
      }
      if (!prover.Verify(digest)) {
        vs = VerifyStatus::FAILED;
      }
    }
    gettimeofday(&t1, NULL);
    auto elapsed = ((t1.tv_sec - t0.tv_sec)*1000000 +
                    (t1.tv_usec - t0.tv_usec));
    std::cout << "verify " << elapsed << " " << reply.ByteSizeLong() << " " << reply.qproof_size() << " " << vs << std::endl;
#endif

    Promise *w = waiting;
    waiting = NULL;
    w->Reply(reply.status(), vs, unverified_keys, estimate_blocks);
  }
}

void
ShardClient::GetTimeout()
{
  if (waiting != NULL) {
    Promise *w = waiting;
    waiting = NULL;
    w->Reply(REPLY_TIMEOUT);
  }
}

/* Callback from a shard replica on prepare operation completion. */
void
ShardClient::PrepareCallback(const string &request_str, const string &reply_str)
{
  Reply reply;

  timeval t;
  gettimeofday(&t, NULL);
  // std::cout << "plat " << (t.tv_sec * 1000000 + t.tv_usec - psend) << std::endl;
  // std::cout << "pressize " << reply_str.size() << std::endl;
  reply.ParseFromString(reply_str);

  if (waiting != NULL) {
    Promise *w = waiting;
    waiting = NULL;
    if (reply.has_timestamp()) {
      w->Reply(reply.status(), Timestamp(reply.timestamp(), 0));
    } else {
      w->Reply(reply.status(), Timestamp());
    }
  }
}

/* Callback from a shard replica on abort operation completion. */
void
ShardClient::AbortCallback(const string &request_str, const string &reply_str)
{
  // ABORTs always succeed.
  Reply reply;
  reply.ParseFromString(reply_str);
  ASSERT(reply.status() == REPLY_OK);

  ASSERT(blockingBegin != NULL);
  blockingBegin->Reply(0);

  if (waiting != NULL) {
    Promise *w = waiting;
    waiting = NULL;
    w->Reply(reply.status());
  }
  }

} // namespace strongstore
