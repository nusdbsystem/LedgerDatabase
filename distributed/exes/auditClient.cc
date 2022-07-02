#include <time.h>
#include <future>

#include "tbb/concurrent_hash_map.h"
#include "boost/thread.hpp"

#include "store/common/truetime.h"
#include "store/strongstore/client.h"

using namespace std;

// ycsb
double readperc, writeperc;

// audit
std::map<int, uint64_t> progress;

enum Mode{
  MODE_UNKNOWN,
  MODE_TAPIR,
  MODE_WEAK,
  MODE_STRONG
};

std::string default_val[10] = {"aaaaaaaaaaaaaaaaaaaa",
                 "bbbbbbbbbbbbbbbbbbbb",
                 "cccccccccccccccccccc",
                 "dddddddddddddddddddd",
                 "eeeeeeeeeeeeeeeeeeee",
                 "ffffffffffffffffffff",
                 "gggggggggggggggggggg",
                 "hhhhhhhhhhhhhhhhhhhh",
                 "iiiiiiiiiiiiiiiiiiii",
                 "jjjjjjjjjjjjjjjjjjjj"};

void millisleep(size_t t) {
  timespec req;
  req.tv_sec = (int) t/1000; 
  req.tv_nsec = (int64_t)((t%1000)*1e6); 
  nanosleep(&req, NULL); 
}

int verifyThread(strongstore::Client* client, int idx, int tLen, int wPer, int duration, size_t timeout) {
  struct timeval t0, t1, t2, t3;
  size_t nAudit = 0;

  gettimeofday(&t0, NULL);
  size_t skip = 100;
  while (1) {
    if (skip > 0) {
      --skip;
    } else {
      millisleep(timeout);
    }

    gettimeofday(&t2, NULL);
    bool vs = client->Audit(progress);
    gettimeofday(&t3, NULL);
    long latency = (t3.tv_sec - t2.tv_sec)*1000000 +
                   (t3.tv_usec - t2.tv_usec);
    ++nAudit;

    fprintf(stderr, "%ld %ld.%06ld %ld.%06ld %ld %d %d\n", nAudit,
        t2.tv_sec, t2.tv_usec, t3.tv_sec, t3.tv_usec, latency, vs?1:0, 3);

    gettimeofday(&t1, NULL);
    if (((t1.tv_sec-t0.tv_sec)*1000000 + (t1.tv_usec-t0.tv_usec)) >
        duration*1000000) {
      std::cout << "Auditing terminated" << std::endl; 
      break;
    }
  }
}

int main(int argc, char **argv) {
  const char *configPath = NULL;
  int duration = 10;
  int nShards = 1;
  int tLen = 10;
  int wPer = 50; // Out of 100
  int closestReplica = -1; // Closest replica id.
  int skew = 0; // difference between real clock and TrueTime
  int error = 0; // error bars
  int idx = 0;
  size_t timeout = 0;

  int opt;
  while ((opt = getopt(argc, argv, "c:d:N:l:w:k:f:m:e:s:z:r:i:t:")) != -1) {
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

    default:
      fprintf(stderr, "Unknown argument %s\n", argv[optind]);
      break;
    }
  }
  strongstore::Client *client = new strongstore::Client(strongstore::MODE_OCC, configPath,
      nShards, closestReplica, TrueTime(skew, error));

  std::vector<std::future<int>> actual_ops;
  actual_ops.emplace_back(std::async(std::launch::async, verifyThread, client, idx, tLen, wPer, duration, timeout));
}