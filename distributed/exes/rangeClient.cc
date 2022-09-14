#include <time.h>
#include <future>

#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_queue.h"
#include "boost/thread.hpp"

#include "distributed/store/common/truetime.h"
#include "distributed/store/strongstore/client.h"

using namespace std;

struct Task {
  int from;
  int to;
};

tbb::concurrent_queue<Task> task_queue;
bool running = true;

// verification
uint64_t curr_block = 0;
boost::shared_mutex lck;
std::map<int, std::map<uint64_t, std::set<std::string>>> verifymap;

std::string default_val[10] = { "aaaaaaaaaaaaaaaaaaaa",
                                "bbbbbbbbbbbbbbbbbbbb",
                                "cccccccccccccccccccc",
                                "dddddddddddddddddddd",
                                "eeeeeeeeeeeeeeeeeeee",
                                "ffffffffffffffffffff",
                                "gggggggggggggggggggg",
                                "hhhhhhhhhhhhhhhhhhhh",
                                "iiiiiiiiiiiiiiiiiiii",
                                "jjjjjjjjjjjjjjjjjjjj" };

void millisleep(size_t t) {
  timespec req;
  req.tv_sec = (int) t/1000; 
  req.tv_nsec = (int64_t)((t%1000)*1e6); 
  nanosleep(&req, NULL); 
}

int taskGenerator(int clientid, int tid, int timeout) {
  timeval t0;
  gettimeofday(&t0, NULL);
  srand(t0.tv_sec*10000000000 + t0.tv_usec*10000 + clientid*10 + tid);

  while (1) {
    millisleep(timeout);
    if (!running) {
      std::cout << "task gen thread terminated" << std::endl;
      return 0;
    }
    
    Task task;
    task.from = rand() % 89999 + 10000;
    auto range = rand() % 10 + 10;
    task.to = task.from + range;
    task_queue.push(task);
  }
  return 0;
}

#ifndef AMZQLDB

int verifyThread(strongstore::Client* client, int idx, int tLen, int wPer, int duration, size_t timeout) {
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
        t0.tv_sec, t0.tv_usec, t1.tv_sec, t1.tv_usec, latency, vs?1:0, 3);
    
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

int
txnThread(strongstore::Client* client, int idx, int tLen, int wPer, int rPer, int duration) {
  // Read in the keys from a file.
  struct timeval t0, t1, t2;
  size_t nTransactions = 0;

  gettimeofday(&t0, NULL);

  while (1) {
    Task task;
    while (!task_queue.try_pop(task));

    client->Begin();
    gettimeofday(&t1, NULL);
    
    std::map<int, std::map<uint64_t, std::vector<std::string>>> verify;
    std::map<std::string, std::string> values;
  
    auto status = client->GetRange(std::to_string(task.from), std::to_string(task.to),
        values, verify);

    gettimeofday(&t2, NULL);
    long latency = (t2.tv_sec - t1.tv_sec)*1000000 +
                   (t2.tv_usec - t1.tv_usec);

    ++nTransactions;

    fprintf(stderr, "%ld %ld.%06ld %ld.%06ld %ld %d %d\n", nTransactions,
        t1.tv_sec, t1.tv_usec, t2.tv_sec, t2.tv_usec, latency, (status == 0), 0);

#ifndef AMZQLDB
    {
        boost::unique_lock<boost::shared_mutex> write(lck);
        for (auto& replicas : verify) {
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
    gettimeofday(&t1, NULL);
    if (((t1.tv_sec-t0.tv_sec)*1000000 + (t1.tv_usec-t0.tv_usec)) >
        duration*1000000) {
      std::cout << "txn thread terminated" << std::endl;
      running = false;
      break;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *configPath = NULL;
  int duration = 10;
  int nShards = 1;
  int tLen = 10;
  int wPer = 50; // Out of 100
  int rPer = 50; // Out of 100
  int closestReplica = -1; // Closest replica id.
  int skew = 0; // difference between real clock and TrueTime
  int error = 0; // error bars
  int idx = 0;
  int txn_rate = 10;
  size_t timeout = 0;

  int opt;
  while ((opt = getopt(argc, argv, "c:d:N:l:w:g:k:f:m:e:s:z:r:i:t:x:")) != -1) {
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
      char *strtolPtr;
      tLen = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (tLen <= 0)) {
        fprintf(stderr, "option -l requires a numeric arg\n");
      }
      break;
    }

    case 'w': // Percentage of writes (out of 100)
    {
      char *strtolPtr;
      wPer = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (wPer < 0 || wPer > 100)) {
        fprintf(stderr, "option -w requires a arg b/w 0-100\n");
      }
      break;
    }

    case 'g': // Percentage of gets (out of 100)
    {
      char *strtolPtr;
      rPer = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (wPer < 0 || wPer > 100)) {
        fprintf(stderr, "option -w requires a arg b/w 0-100\n");
      }
      break;
    }

    case 'k': // Number of keys to operate on.
    {
      break;
    }

    case 's': // Simulated clock skew.
    {
      char *strtolPtr;
      skew = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0))
      {
        fprintf(stderr,
            "option -s requires a numeric arg\n");
      }
      break;
    }

    case 'e': // Simulated clock error.
    {
      char *strtolPtr;
      error = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0))
      {
        fprintf(stderr,
            "option -e requires a numeric arg\n");
      }
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

    case 't': // index of client
    {
      char *strtolPtr;
      timeout = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (timeout < 0)) {
        fprintf(stderr, "option -t requires a numeric arg\n");
      }
      break; 
    }

    case 'x': // transaction rate
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
  strongstore::Client *client = new strongstore::Client(strongstore::MODE_OCC, configPath,
      nShards, closestReplica, TrueTime(skew, error));

  std::vector<std::future<int>> actual_ops;
  int numThread = 10;
  int interval = 1000 / txn_rate;
  for (int i = 0; i < numThread; ++i) {
    actual_ops.emplace_back(std::async(std::launch::async, taskGenerator, idx, i, interval));
  }
  actual_ops.emplace_back(std::async(std::launch::async, txnThread, client, idx, tLen, wPer, rPer, duration));
#ifndef AMZQLDB
  actual_ops.emplace_back(std::async(std::launch::async, verifyThread, client, idx, tLen, wPer, duration, timeout));
#endif
}
