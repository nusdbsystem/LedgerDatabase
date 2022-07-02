#include <numeric>
#include <future>

#include "boost/algorithm/string.hpp"
#include "boost/thread.hpp"
#include "tbb/concurrent_hash_map.h"

#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/strongstore/client.h"

using namespace std;

struct Task {
  int op;
  int acc1;
  int acc2;
  int balance;
};

tbb::concurrent_queue<Task> task_queue;

// verification
bool running = true;
uint64_t curr_block = 0;
boost::shared_mutex lck;
std::map<int, std::map<uint64_t, std::set<std::string>>> verifymap;

const std::string saving = "savingStore_";
const std::string checking = "checkingStore_";

bool Amalgamate(unsigned int acc1, unsigned int acc2, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    client->Begin();
    std::string acc1_key = saving + std::to_string(acc1);
    std::string acc2_key = checking + std::to_string(acc2);
    client->BufferKey(acc1_key);
    client->BufferKey(acc2_key);
    std::map<string, string> values;
    client->BatchGet(values, unverified_keys);
    unsigned int bal1 = stoul(values[acc1_key]);
    unsigned int bal2 = stoul(values[acc2_key]);
    client->Put(checking + std::to_string(acc1), "0");
    client->Put(saving + std::to_string(acc2), std::to_string(bal1 + bal2));
    return client->Commit();
}

bool GetBalance(unsigned int acc, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    client->Begin();
    std::string sav_key = saving + std::to_string(acc);
    std::string chk_key = checking + std::to_string(acc);
    client->BufferKey(sav_key);
    client->BufferKey(chk_key);
    std::map<string, string> values;
    auto status = client->BatchGet(values, unverified_keys);
    if (status != REPLY_OK) return false;
    unsigned int bal1 = stoul(values[sav_key]);
    unsigned int bal2 = stoul(values[chk_key]);
    unsigned int balance = bal1 + bal2;
    return true;
}

bool UpdateBalance(unsigned int acc, unsigned int amount, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    client->Begin();
    std::string chk_key = checking + std::to_string(acc);
    client->BufferKey(chk_key);
    std::map<string, string> values;
    client->BatchGet(values, unverified_keys);
    unsigned int bal1 = stoul(values[chk_key]);
    client->Put(chk_key, std::to_string(bal1 + amount));
    return client->Commit();
}

bool UpdateSaving(unsigned int acc, unsigned int amount, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    client->Begin();
    std::string sav_key = saving + std::to_string(acc);
    client->BufferKey(sav_key);
    std::map<string, string> values;
    client->BatchGet(values, unverified_keys);
    unsigned int bal1 = stoul(values[sav_key]);
    client->Put(sav_key, std::to_string(bal1 + amount));
    return client->Commit();
}

bool SendPayment(unsigned int acc1, unsigned int acc2, unsigned int amount, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    std::string chk_key1 = checking + std::to_string(acc1);
    std::string chk_key2 = checking + std::to_string(acc2);
    client->Begin();
    client->BufferKey(chk_key1);
    client->BufferKey(chk_key2);
    std::map<string, string> values;
    client->BatchGet(values, unverified_keys);
    unsigned int bal1 = stoul(values[chk_key1]);
    unsigned int bal2 = stoul(values[chk_key2]);
    bal1 -= amount;
    bal2 += amount;
    client->Put(chk_key1, std::to_string(bal1));
    client->Put(chk_key2, std::to_string(bal2));
    return client->Commit();
}

bool WriteCheck(unsigned int acc, unsigned int amount, strongstore::Client* client,
        std::map<int, std::map<uint64_t, std::vector<std::string>>>& unverified_keys) {
    std::string sav_key = saving + std::to_string(acc);
    std::string chk_key = checking + std::to_string(acc);
    client->Begin();
    client->BufferKey(sav_key);
    client->BufferKey(chk_key);
    std::map<string, string> values;
    client->BatchGet(values, unverified_keys);
    unsigned int bal1 = stoul(values[chk_key]);
    unsigned int bal2 = stoul(values[sav_key]);

    if (amount < bal1 + bal2) {
        client->Put(chk_key, std::to_string(bal1 - amount - 1));
    } else {
        client->Put(chk_key, std::to_string(bal1 - amount));
    }
    return client->Commit();
}

int
txnThread(strongstore::Client* client, int duration) {

  // Read in the keys from a file.
  struct timeval t0, t1, t2;
  int nTransactions = 0;
  gettimeofday(&t0, NULL);
  while (1) {
    Task task;
    while (!task_queue.try_pop(task));

    bool status;
    std::map<int, std::map<uint64_t, std::vector<std::string>>> unverified_keys;
    gettimeofday(&t1, NULL);
    switch (task.op) {
      case 1:
        std::cout << "Amalgamate ..." << std::endl;
        status = Amalgamate(task.acc1, task.acc2, client, unverified_keys);
        break;
      case 2:
        std::cout << "GetBalance ..." << std::endl;
        status = GetBalance(task.acc1, client, unverified_keys);
        break;
      case 3:
        std::cout << "UpdateBalance ..." << std::endl;
        status = UpdateBalance(task.acc1, 0, client, unverified_keys);
        break;
      case 4:
        std::cout << "UpdateSaving ..." << std::endl;
        status = UpdateSaving(task.acc1, 0, client, unverified_keys);
        break;
      case 5:
        std::cout << "SendPayment ..." << std::endl;
        status = SendPayment(task.acc1, task.acc2, 0, client, unverified_keys);
        break;
      case 6:
        std::cout << "WriteCheck ..." << std::endl;
        status = WriteCheck(task.acc1, 0, client, unverified_keys);
        break;
      default:
        std::cout << "Unknown operation: " << task.op << std::endl;
        break;
    }
    gettimeofday(&t2, NULL);

    long latency = (t2.tv_sec - t1.tv_sec)*1000000 +
                   (t2.tv_usec - t1.tv_usec);

    nTransactions++;

    fprintf(stderr, "%d %ld.%06ld %ld.%06ld %ld %d %d\n", nTransactions,
        t1.tv_sec, t1.tv_usec, t2.tv_sec, t2.tv_usec, latency, status?1:0, task.op);

#ifndef AMZQLDB
    {
        boost::unique_lock<boost::shared_mutex> write(lck);
        for (auto& replicas : unverified_keys) {
            if (verifymap.find(replicas.first) != verifymap.end()) {
                for (auto& blocks : replicas.second) {
                    if (verifymap[replicas.first].find(blocks.first) != verifymap[replicas.first].end()) {
                        for (auto& keys : blocks.second) {
                            verifymap[replicas.first][blocks.first].emplace(keys);
                        }
                    } else {
                        std::set<std::string> ks(blocks.second.begin(), blocks.second.end());
                        verifymap[replicas.first].emplace(blocks.first, ks);
                    }
                }
            } else {
                std::map<uint64_t, std::set<std::string>> blocks;
                for (auto& block : replicas.second) {
                  std::set<std::string> keys;
                  for (auto& k : block.second) {
                    keys.emplace(k);
                  }
                  blocks.emplace(block.first, keys);
                }
                verifymap.emplace(replicas.first, blocks);
            }
        }
    }
#endif

    if (((t2.tv_sec-t0.tv_sec)*1000000 + (t2.tv_usec-t0.tv_usec)) >
        duration*1000000) {
      running = false; 
      break;
    }
  }
  return 0;
}

void millisleep(size_t t) {
  timespec req;
  req.tv_sec = (int) t/1000; 
  req.tv_nsec = (int64_t)((t%1000)*1e6); 
  nanosleep(&req, NULL); 
}

#ifndef AMZQLDB
int verifyThread(strongstore::Client* client, size_t timeout) {
  while (1) {
    millisleep(timeout);
    if (!running) {
      std::cout << "verify thread terminated" << std::endl;
      return 0;
    }

    std::map<int, std::map<uint64_t, std::vector<std::string>>> unverified_keys;
    {
      boost::shared_lock<boost::shared_mutex> read(lck);
      if (verifymap.size() == 0) continue;
      for (auto& replica : verifymap) {
        std::map<uint64_t, std::vector<std::string>> blocks;
        for (auto& block : replica.second) {
          std::vector<std::string> keys;
          for (auto& k : block.second) {
            keys.push_back(k);
          }
          blocks.emplace(block.first, keys);
        }
        unverified_keys.emplace(replica.first, blocks);
      }
    }
    
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    bool vs = client->Verify(unverified_keys);
    gettimeofday(&t1, NULL);
    long latency = (t1.tv_sec - t0.tv_sec)*1000000 +
                   (t1.tv_usec - t0.tv_usec);

    fprintf(stderr, "%ld %ld.%06ld %ld.%06ld %ld %d %d\n", unverified_keys.size(),
        t0.tv_sec, t0.tv_usec, t1.tv_sec, t1.tv_usec, latency, vs?1:0, 9);
    
    {
      boost::unique_lock<boost::shared_mutex> write(lck);
      for (auto& replica : unverified_keys) {
        for (auto& block : replica.second) {
          verifymap[replica.first].erase(block.first);
        }
        if (verifymap[replica.first].empty()) {
          verifymap.erase(replica.first);
        }
      }
    }
  }
}

#endif

int taskGenerator(int timeout) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> op_gen(1, 6);
  std::uniform_int_distribution<> acc_gen(1, 100000);
  std::uniform_int_distribution<> bal_gen(1, 10);

  while (true) {
    millisleep(timeout);
    if (!running) {
      std::cout << "task gen thread terminated" << std::endl;
      return 0;
    }
    Task task;
    task.op = op_gen(gen);
    task.acc1 = acc_gen(gen);
    task.acc2 = acc_gen(gen);
    task.balance = bal_gen(gen);
    task_queue.push(task);
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *configPath = NULL;
  int duration = 10, timeout=1000, nShards = 1, idx = 0, txn_rate = 10;
  int closestReplica = -1; // Closest replica id.

  int opt;
  while ((opt = getopt(argc, argv, "c:d:N:l:w:k:f:m:e:s:z:r:i:t:x:")) != -1) {
    switch (opt) {
    case 'c': // Configuration path
    { 
      configPath = optarg;
      break;
    }

    case 'f': // Generated keys path
    { 
      break;
    }

    case 'N': // Number of shards.
    { 
      char *strtolPtr;
      nShards = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (nShards <= 0)) {
        fprintf(stderr, "option -n requires a numeric arg\n");
      }
      break;
    }

    case 'd': // Duration in seconds to run.
    { 
      char *strtolPtr;
      duration = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (duration <= 0)) {
        fprintf(stderr, "option -n requires a numeric arg\n");
      }
      break;
    }

    case 'l': // Length of each transaction (deterministic!)
    {
      break;
    }

    case 'w': // Percentage of writes (out of 100)
    {
      break;
    }

    case 'k': // Number of keys to operate on.
    {
      break;
    }

    case 's': // Simulated clock skew.
    {
      break;
    }

    case 'e': // Simulated clock error.
    {
      break;
    }

    case 'z': // Zipf coefficient for key selection.
    {
      break;
    }

    case 'r': // Preferred closest replica.
    {
      char *strtolPtr;
      closestReplica = strtod(optarg, &strtolPtr);
      if ((*optarg == '\0') || (*strtolPtr != '\0'))
      {
        fprintf(stderr,
            "option -r requires a numeric arg\n");
      }
      break;
    }

    case 'm': // Mode to run in [occ/lock/...]
    {
      break;
    }

    case 'i': // index of client
    {
      char *strtolPtr;
      idx = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (idx < 0)) {
        fprintf(stderr, "option -i requires a numeric arg\n");
      }
      break; 
    }

    case 't': // timeout
    {
      char *strtolPtr;
      timeout = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (timeout < 0)) {
        fprintf(stderr, "option -t requires a numeric arg\n");
      }
      break; 
    }

    case 'x':  // txn rate
    {
      char *strtolPtr;
      txn_rate = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (txn_rate < 0)) {
        fprintf(stderr, "option -x requires a numeric arg\n");
      }
      break; 
    }

    default:
      fprintf(stderr, "Unknown argument %s\n", argv[optind]);
      break;
    }
  }

  strongstore::Client *client = new strongstore::Client(strongstore::MODE_OCC,
      configPath, nShards, closestReplica, TrueTime(0, 0));

  std::vector<std::future<int>> actual_ops;
  int numThread = 10;
  int interval = 1000 / txn_rate;
  for (int i = 0; i < numThread; ++i) {
    actual_ops.emplace_back(std::async(std::launch::async, taskGenerator, interval));
  }
  actual_ops.emplace_back(std::async(std::launch::async, txnThread, client, duration));
#ifndef AMZQLDB
  actual_ops.emplace_back(std::async(std::launch::async, verifyThread, client, timeout));
#endif
}
