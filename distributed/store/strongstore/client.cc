#include "distributed/store/strongstore/client.h"

using namespace std;

namespace strongstore {

Client::Client(Mode mode, string configPath, int nShards,
                int closestReplica, TrueTime timeServer)
    : transport(0.0, 0.0, 0), mode(mode), timeServer(timeServer)
{
    // Initialize all state here;
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }
    t_id = (client_id/10000)*10000;

    nshards = nShards;
    bclient.reserve(nshards);


    /* Start a client for time stamp server. */
    if (mode == MODE_OCC) {
        string tssConfigPath = configPath + ".tss.config";
        ifstream tssConfigStream(tssConfigPath);
        if (tssConfigStream.fail()) {
            fprintf(stderr, "unable to read configuration file: %s\n",
                    tssConfigPath.c_str());
        }
        transport::Configuration tssConfig(tssConfigStream);
        tss = new replication::vr::VRClient(tssConfig, &transport);
    }

    /* Start a client for each shard. */
    for (int i = 0; i < nShards; i++) {
        string shardConfigPath = configPath + to_string(i) + ".config";
        ShardClient *shardclient = new ShardClient(mode, shardConfigPath,
            &transport, client_id, i, closestReplica);
        bclient[i] = new BufferClient(shardclient);
    }

    /* Run the transport in a new thread. */
    clientTransport = new thread(&Client::run_client, this);

    }

Client::~Client()
{
    transport.Stop();
    delete tss;
    for (auto& b : bclient) {
        delete b;
    }
    clientTransport->join();
}

/* Runs the transport event loop. */
void
Client::run_client()
{
    transport.Run();
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 *
 * Return a TID for the transaction.
 */
void
Client::Begin()
{
    t_id++;
    participants.clear();
    commit_sleep = -1;
    for (int i = 0; i < nshards; i++) {
        bclient[i]->Begin(t_id);
    }
}

int Client::GetNVersions(const string& key, size_t n) {
    // Contact the appropriate shard to get the value.
    int i = key_to_shard(key, nshards);

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
        participants.insert(i);
    }

    Promise promise(GET_TIMEOUT);
    bclient[i]->GetNVersions(key, n, &promise);

    return promise.GetReply();
}

int Client::Get(const string &key)
{
    // Contact the appropriate shard to get the value.
    int i = key_to_shard(key, nshards);

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
        participants.insert(i);
    }

    Promise promise(GET_TIMEOUT);
    bclient[i]->Get(key, &promise);

    return promise.GetReply();
}

int Client::BatchGet(std::map<std::string, std::string>& values, std::map<int,
    std::map<uint64_t, std::vector<std::string>>>& keys) {
  int status = REPLY_OK;

  map<int, Promise *> promises;

  for (auto p : participants) {
    promises.emplace(p, new Promise(PREPARE_TIMEOUT));
    bclient[p]->BatchGet(promises[p]);
  }

  for (auto& entry : promises) {
    Promise *p = entry.second;
    if (p->GetReply() == REPLY_OK) {
      values.insert(p->getValues().begin(), p->getValues().end());
      for (size_t i = 0; i < p->EstimateBlockSize(); ++i) {
        auto block = p->GetEstimateBlock(i);
        auto key = p->GetUnverifiedKey(i);
        if (keys.find(entry.first) != keys.end()) {
          if (keys[entry.first].find(block) != keys[entry.first].end()) {
            keys[entry.first][block].emplace_back(key);
          } else {
            keys[entry.first].emplace(block, std::vector<std::string>{key});
          }
        } else {
          std::map<uint64_t, std::vector<std::string>> innermap;
          innermap.emplace(block, std::vector<std::string>{key});
          keys.emplace(entry.first, innermap);
        }
      }
    } else {
      status = p->GetReply();
    }
    delete p;
  }

  return status;
}

int Client::BatchGet(std::map<std::string, std::string>& values) {
  int status = REPLY_OK;

  map<int, Promise *> promises;

  for (auto p : participants) {
    promises.emplace(p, new Promise(PREPARE_TIMEOUT));
    bclient[p]->BatchGet(promises[p]);
  }

  for (auto& entry : promises) {
    auto p = entry.second;
    if (p->GetReply() == REPLY_OK) {
      values.insert(p->getValues().begin(), p->getValues().end());
    } else {
      status = p->GetReply();
    }
    delete p;
  }

  return status;
}

int Client::GetRange(const string& from, const string& to,
    std::map<std::string, std::string>& values, std::map<int,
    std::map<uint64_t, std::vector<std::string>>>& keys) {
  int status = REPLY_OK;
  map<int, Promise *> promises;

  for (int i = 0; i < nshards; ++i) {
    promises.emplace(i, new Promise(GET_TIMEOUT));
    bclient[i]->GetRange(from, to, promises[i]);
  }

  for (auto entry : promises) {
    auto p = entry.second;
    if (p->GetReply() == REPLY_OK) {
      values.insert(p->getValues().begin(), p->getValues().end());
      for (size_t i = 0; i < p->EstimateBlockSize(); ++i) {
        auto block = p->GetEstimateBlock(i);
        auto key = p->GetUnverifiedKey(i);
        if (keys.find(entry.first) != keys.end()) {
          if (keys[entry.first].find(block) != keys[entry.first].end()) {
            keys[entry.first][block].emplace_back(key);
          } else {
            keys[entry.first].emplace(block, std::vector<std::string>{key});
          }
        } else {
          std::map<uint64_t, std::vector<std::string>> innermap;
          innermap.emplace(block, std::vector<std::string>{key});
          keys.emplace(entry.first, innermap);
        }
      }
    } else {
      status = p->GetReply();
    }
    delete p;
  }

  return status;
}

void Client::BufferKey(const std::string& key) {
  int i = key_to_shard(key, nshards);
  // If needed, add this shard to set of participants and send BEGIN.
  if (participants.find(i) == participants.end()) {
      participants.insert(i);
  }
  bclient[i]->BufferKey(key);
}

/* Sets the value corresponding to the supplied key. */
int
Client::Put(const string &key, const string &value)
{
    // Contact the appropriate shard to set the value.
    int i = key_to_shard(key, nshards);

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
        participants.insert(i);
    }

    Promise promise(PUT_TIMEOUT);

    // Buffering, so no need to wait.
    bclient[i]->Put(key, value, &promise);
    return promise.GetReply();
}

int
Client::Prepare(uint64_t &ts)
{
    int status;

    // 0. go get a timestamp for OCC
    if (mode == MODE_OCC) {
        // Have to go to timestamp server
        unique_lock<mutex> lk(cv_m);

        transport.Timer(0, [=]() {
		        tss->Invoke("", bind(&Client::tssCallback, this,
                placeholders::_1,
                placeholders::_2));
	      });

        cv.wait(lk);
        ts = stol(replica_reply, NULL, 10);
    }

    // 1. Send commit-prepare to all shards.
    list<Promise *> promises;

    for (auto& p : participants) {
        promises.push_back(new Promise(PREPARE_TIMEOUT));
        bclient[p]->Prepare(Timestamp(),promises.back());
    }

    // 2. Wait for reply from all shards. (abort on timeout)

    status = REPLY_OK;
    for (auto& p : promises) {
        // If any shard returned false, abort the transaction.
        if (p->GetReply() != REPLY_OK) {
            if (status != REPLY_FAIL) {
                status = p->GetReply();
            }
        }
        // Also, find the max of all prepare timestamp returned.
        if (p->GetTimestamp().getTimestamp() > ts) {
            ts = p->GetTimestamp().getTimestamp();
        }
        delete p;
    }
    return status;
}

/* Attempts to commit the ongoing transaction. */
bool
Client::Commit()
{
    // Implementing 2 Phase Commit
    uint64_t ts = 0;
    int status;

    for (int i = 0; i < COMMIT_RETRIES; i++) {
        status = Prepare(ts);
        if (status == REPLY_OK || status == REPLY_FAIL) {
            break;
        }
    }

    if (status == REPLY_OK) {
        // For Spanner like systems, calculate timestamp.
        if (mode == MODE_SPAN_OCC || mode == MODE_SPAN_LOCK) {
            uint64_t now, err;
            struct timeval t1, t2;

            gettimeofday(&t1, NULL);
            timeServer.GetTimeAndError(now, err);

            if (now > ts) {
                ts = now;
            } else {
                uint64_t diff = ((ts >> 32) - (now >> 32))*1000000 +
                        ((ts & 0xffffffff) - (now & 0xffffffff));
                err += diff;
            }

            commit_sleep = (int)err;

            // how good are we at waking up on time?
                        if (err > 1000000)
                Warning("Sleeping for too long! %lu; now,ts: %lu,%lu", err, now, ts);
            if (err > 150) {
                usleep(err-150);
            }
            // fine grained busy-wait
            while (1) {
                gettimeofday(&t2, NULL);
                if ((t2.tv_sec-t1.tv_sec)*1000000 +
                    (t2.tv_usec-t1.tv_usec) > (int64_t)err) {
                    break;
                }
            }
        }

        // Send commits

        for (auto& p : participants) {
                        bclient[p]->Commit(ts);
        }
        return true;
    }

    // 4. If not, send abort to all shards.
    Abort();
    return false;
}

bool Client::Commit(std::map<int, std::map<uint64_t, std::vector<std::string>>>& keys) {
  // Implementing 2 Phase Commit
    uint64_t ts = 0;
    int status;

    for (int i = 0; i < COMMIT_RETRIES; i++) {
        status = Prepare(ts);
        if (status == REPLY_OK || status == REPLY_FAIL) {
            break;
        }
    }

    if (status == REPLY_OK) {
        // For Spanner like systems, calculate timestamp.
        if (mode == MODE_SPAN_OCC || mode == MODE_SPAN_LOCK) {
            uint64_t now, err;
            struct timeval t1, t2;

            gettimeofday(&t1, NULL);
            timeServer.GetTimeAndError(now, err);

            if (now > ts) {
                ts = now;
            } else {
                uint64_t diff = ((ts >> 32) - (now >> 32))*1000000 +
                        ((ts & 0xffffffff) - (now & 0xffffffff));
                err += diff;
            }

            commit_sleep = (int)err;

            // how good are we at waking up on time?
                        if (err > 1000000)
                Warning("Sleeping for too long! %lu; now,ts: %lu,%lu", err, now, ts);
            if (err > 150) {
                usleep(err-150);
            }
            // fine grained busy-wait
            while (1) {
                gettimeofday(&t2, NULL);
                if ((t2.tv_sec-t1.tv_sec)*1000000 +
                    (t2.tv_usec-t1.tv_usec) > (int64_t)err) {
                    break;
                }
            }
        }

        // Send commits

        map<int, Promise *> promises;
        for (auto& p : participants) {
                        promises.emplace(p, new Promise(PREPARE_TIMEOUT));
            bclient[p]->Commit(ts, promises[p]);
        }
        for (auto& entry : promises) {
            Promise *p = entry.second;
            p->GetReply();
            for (size_t i = 0; i < p->EstimateBlockSize(); ++i) {
              auto block = p->GetEstimateBlock(i);
              auto key = p->GetUnverifiedKey(i);
              if (keys.find(entry.first) != keys.end()) {
                if (keys[entry.first].find(block) != keys[entry.first].end()) {
                  keys[entry.first][block].emplace_back(key);
                } else {
                  keys[entry.first].emplace(block, std::vector<std::string>{key});
                }
              } else {
                std::map<uint64_t, std::vector<std::string>> innermap;
                innermap.emplace(block, std::vector<std::string>{key});
                keys.emplace(entry.first, innermap);
              }
            }
            delete p;
        }
        return true;
    }

    // 4. If not, send abort to all shards.
    Abort();
    return false;
}

/* Aborts the ongoing transaction. */
void
Client::Abort()
{
        for (auto& p : participants) {
        bclient[p]->Abort();
    }
}

bool Client::Verify(std::map<int, std::map<uint64_t, std::vector<std::string>>>& keys) {
    // Contact the appropriate shard to set the value.
    bool is_successful = true;

#ifndef AMZQLDB
    list<Promise*> promises;
    size_t nkeys = 0;

    for (auto k = keys.begin(); k != keys.end();) {
        bool verifiable = true;
        for (auto block = k->second.begin(); block != k->second.end();) {
            if (!verifiable) {
                block = k->second.erase(block);
            } else {
                while (block->second.size() > 10000) {
                  promises.push_back(new Promise());
                  std::vector<std::string> newvec(block->second.begin(),
                      block->second.begin() + 10000);
                  if (!bclient[k->first]->Verify(block->first, newvec,
                      promises.back())) {
                    verifiable = false;
                    promises.pop_back();
                    break;
                  }
                  block->second.erase(block->second.begin(), block->second.begin() + 10000);
                  nkeys += 10000;
                }

                promises.push_back(new Promise());
                if (!verifiable || !bclient[k->first]->Verify(block->first,
                                            block->second,
                                            promises.back())) {
                    verifiable = false;
                    block = k->second.erase(block);
                    promises.pop_back();
                } else {
                    nkeys += block->second.size();
                    ++block;
                }
            }
        }
        if (k->second.size() == 0) {
            k = keys.erase(k);
        } else {
            ++k;
        }
    }
    std::cout << "verifynkeys " << nkeys << std::endl;

    for (auto& p : promises) {
        if (p->GetReply() != REPLY_OK ||
            p->GetVerifyStatus() != VerifyStatus::PASS) {
          is_successful = false;
        }
        delete p;
    }
#endif

    return is_successful;
}

bool Client::Audit(std::map<int, uint64_t>& seqs) {
    // Contact the appropriate shard to set the value.
    bool status = true;
    list<Promise *> promises;

    if (seqs.size() == 0) {
      for(size_t n = 0; n < nshards; ++n) {
        seqs.emplace(n, 0);
      }
    }

    for (auto k = seqs.begin(); k != seqs.end(); ++k) {
        promises.push_back(new Promise());
        bclient[k->first]->Audit(k->second, promises.back());
    }

    int n = 0;
    for (auto p : promises) {
        if (p->GetReply() != REPLY_OK) {
          status = false;
        } else if (p->GetVerifyStatus() != VerifyStatus::UNVERIFIED) {
          ++seqs[n];
        }
        ++n;
        delete p;
    }
    return status;
}


/* Return statistics of most recent transaction. */
vector<int>
Client::Stats()
{
    vector<int> v;
    return v;
}

/* Callback from a tss replica upon any request. */
void
Client::tssCallback(const string &request, const string &reply)
{
    lock_guard<mutex> lock(cv_m);

    // Copy reply to "replica_reply".
    replica_reply = reply;

    // Wake up thread waiting for the reply.
    cv.notify_all();
}

} // namespace strongstore
