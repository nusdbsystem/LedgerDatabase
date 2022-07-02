#include "store/strongstore/server.h"

namespace strongstore {

using namespace std;
using namespace proto;

Server::Server(Mode mode, uint64_t skew, uint64_t error, int index, bool sp, int timeout) :
    mode(mode), stored_procedure(sp)
{
    timeServer = TrueTime(skew, error);
    std::string db_path = "/tmp/replica" + std::to_string(index) + ".store";
    std::cout << db_path << std::endl;
    switch (mode) {
    case MODE_LOCK:
    case MODE_SPAN_LOCK:
        break;
    case MODE_OCC:
    case MODE_SPAN_OCC:
        store = new strongstore::OCCStore(db_path, timeout);
        break;
    default:
        NOT_REACHABLE();
    }
}

Server::~Server()
{
    delete store;
}

void
Server::LeaderUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2)
{

    Request request;
    Reply reply;
    int status = 0;
    
    request.ParseFromString(str1);

    switch (request.op()) {
    case strongstore::proto::Request::BATCH_GET:                               
    {                                                                           
        //std::cout << "read ";                                                                                                                                                                       
        std::vector<std::string> keys;                                          
        for (size_t i = 0; i < request.batchget().keys_size(); ++i) {           
          keys.push_back(request.batchget().keys(i));                           
        }                                                                       
        status = store->BatchGet(request.txnid(), keys, &reply);                                                                                  
                                                                                
        if (status == 0) {                                                      
            replicate = true;                                                   
        } else {                                                                
            replicate = false;                                                  
        }                                                                       
        reply.set_status(status);                                               
        reply.SerializeToString(&str2);                                         
        break;                                                                  
    }
    case strongstore::proto::Request::PREPARE:
    {
        //std::cout << "prepare ";
        status = store->Prepare(request.txnid(),
                                Transaction(request.prepare().txn()));
        if (status == 0) {
            replicate = true;
            if (mode == MODE_SPAN_LOCK || mode == MODE_SPAN_OCC) {
                // request.mutable_prepare()->set_timestamp(timeServer.GetTime());
                reply.set_timestamp(timeServer.GetTime());
            }
        } else {
            replicate = false;
        }
        reply.set_status(status);
        reply.SerializeToString(&str2);
        break;
    }
    case strongstore::proto::Request::COMMIT:
    {
        //std::cout << "commit ";
        replicate = true;

        auto ver_msg = request.version();
        std::vector<std::pair<std::string, size_t>> ver_keys;
        for (size_t i = 0; i < ver_msg.versionedkeys_size(); ++i) {
          auto k_ver = ver_msg.versionedkeys(i);
          ver_keys.emplace_back(std::make_pair(k_ver.key(), k_ver.nversions()));
        }
        if (stored_procedure) {
          store->Commit(request.txnid(), request.commit().timestamp(), ver_keys,
              &reply);
        } else {
          store->Commit(request.txnid(), request.commit().timestamp(), &reply);
        }
        reply.set_status(status);
        reply.SerializeToString(&str2);
        break;
    }
    case strongstore::proto::Request::ABORT:
    {
        replicate = true;
        store->Abort(request.txnid(), Transaction(request.abort().txn()));
        reply.set_status(status);
        reply.SerializeToString(&str2);
        break;
    }
    default:
        Panic("Unrecognized operation.");
    }
}

/* 
 * Dummy function. No replication supported for now
 */
void
Server::ReplicaUpcall(opnum_t opnum,
              const string &str1,
              string &str2)
{
    Request request;
    Reply reply;
    int status = 0;
    
    request.ParseFromString(str1);

    switch (request.op()) {
    case strongstore::proto::Request::GET:
        return;
    case strongstore::proto::Request::PREPARE:
        break;
    case strongstore::proto::Request::COMMIT:
        break;
    case strongstore::proto::Request::ABORT:
        break;
    default:
        Panic("Unrecognized operation.");
    }
    reply.set_status(status);
    reply.SerializeToString(&str2);
}

void
Server::UnloggedUpcall(const string &str1, string &str2)
{
  Request request;
  Reply reply;
  int status = 0;
  request.ParseFromString(str1);

  if (request.op() == strongstore::proto::Request::VERIFY_GET) {
    std::map<uint64_t, std::vector<std::string>> keys;
    std::vector<std::string> data;
    for (size_t i = 0; i < request.verify().keys_size(); ++i) {
      data.push_back(request.verify().keys(i));
    }
    keys.emplace(request.verify().block(), data);
    status = store->GetProof(keys, &reply);
  } else if (request.op() == strongstore::proto::Request::RANGE) {
    status = store->GetRange(request.range().from(),
                             request.range().to(), &reply);
  } else if (request.op() == strongstore::proto::Request::AUDIT) {
    uint64_t seq = request.audit().seq();
    status = store->GetProof(seq, &reply);
  }

  reply.set_status(status);
  reply.SerializeToString(&str2);
}

void
Server::Load(const std::vector<std::string> &keys,
             const std::vector<std::string> &values,
             const Timestamp timestamp)
{
    store->Load(keys, values, timestamp);
}

} // namespace strongstore

int
main(int argc, char **argv)
{
    int index = -1, timeout;
    unsigned int myShard=0, maxShard=1, nKeys=1, version=1;
    std::string workload;
    bool stored_procedure = false;
    const char *configPath = NULL;
    const char *keyPath = NULL;
    int64_t skew = 0, error = 0;
    strongstore::Mode mode;

    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:i:m:e:s:f:n:N:k:w:t:v:")) != -1) {
        switch (opt) {
        case 'c':
            configPath = optarg;
            break;
            
        case 'i':
        {
            char *strtolPtr;
            index = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (index < 0))
            {
                fprintf(stderr, "option -i requires a numeric arg\n");
            }
            break;
        }
        
        case 'm':
        {
            if (strcasecmp(optarg, "lock") == 0) {
                mode = strongstore::MODE_LOCK;
            } else if (strcasecmp(optarg, "occ") == 0) {
                mode = strongstore::MODE_OCC;
            } else if (strcasecmp(optarg, "span-lock") == 0) {
                mode = strongstore::MODE_SPAN_LOCK;
            } else if (strcasecmp(optarg, "span-occ") == 0) {
                mode = strongstore::MODE_SPAN_OCC;
            } else {
                fprintf(stderr, "unknown mode '%s'\n", optarg);
            }
            break;
        }

        case 's':
        {
            char *strtolPtr;
            skew = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0))
            {
                fprintf(stderr, "option -s requires a numeric arg\n");
            }
            break;
        }

        case 'e':
        {
            char *strtolPtr;
            error = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
            }
            break;
        }

        case 'k':
        {
            char *strtolPtr;
            nKeys = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
            }
            break;
        }

        case 'n':
        {
            char *strtolPtr;
            myShard = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
            }
            break;
        }

        case 'N':
        {
            char *strtolPtr;
            maxShard = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (maxShard <= 0))
            {
                fprintf(stderr, "option -e requires a numeric arg\n");
            }
            break;
        }

        case 'f':   // Load keys from file
        {
            keyPath = optarg;
            break;
        }

        case 'w':
        {
          workload = optarg;
          if (workload.compare("ycsb") == 0) {
            stored_procedure = true;
          }
          break;
        }

        case 'v':
        {
          char *strtolPtr;
          version = strtoul(optarg, &strtolPtr, 10);
          if ((*optarg == '\0') || (*strtolPtr != '\0') || (version <= 0)) {
            fprintf(stderr, "-v Initialize ycsb versions greater than 0\n");
          }
          break;
        }

        case 't':
        {
            char *strtolPtr;
            timeout = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') || (timeout < 0))
            {
                fprintf(stderr, "option -t requires a numeric arg\n");
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
        }
    }

    if (!configPath) {
        fprintf(stderr, "option -c is required\n");
    }

    if (index == -1) {
        fprintf(stderr, "option -i is required\n");
    }

    if (mode == strongstore::MODE_UNKNOWN) {
        fprintf(stderr, "option -m is required\n");
    }

    // Load configuration
    std::ifstream configStream(configPath);
    if (configStream.fail()) {
        fprintf(stderr, "unable to read configuration file: %s\n", configPath);
    }
    transport::Configuration config(configStream);

    if (index >= config.n) {
        fprintf(stderr, "replica index %d is out of bounds; "
                "only %d replicas defined\n", index, config.n);
    }

    TCPTransport transport(0.0, 0.0, 0);

    strongstore::Server server(mode, skew, error, index, stored_procedure, timeout);
    replication::vr::VRReplica replica(config, index, &transport, 1, &server);

    timeval t0, t1;
    gettimeofday(&t0, NULL);
    if (workload.compare("ycsb") == 0) {
      for (int iter = 0; iter < version; ++iter) {
        std::vector<std::string> keys, vals;
        for (size_t i = 0; i <= nKeys; ++i) {
          string key = std::to_string(i);

          uint64_t hash = 5381;
          const char* str = key.c_str();
          for (unsigned int j = 0; j < key.length(); j++) {
            hash = ((hash << 5) + hash) + (uint64_t)str[j];
          }

          if (hash % maxShard == myShard) {
            std::string val = key + std::to_string(iter);
            keys.emplace_back(key);
            vals.emplace_back(val);
          }

          if (keys.size() == 10 || (i == nKeys && keys.size() > 0)) {
            server.Load(keys, vals, Timestamp(iter));
            keys.clear();
            vals.clear();
          }
        }
      }
    } else if (workload.compare("smallbank") == 0) {
      std::vector<std::string> keys, vals;
      for (int i = 1; i <= 100000; ++i) {
        std::string key = "savingStore_" + std::to_string(i);
        uint64_t hash = 5381;
        const char* str = key.c_str();
        for (unsigned int j = 0; j < key.length(); j++) {
          hash = ((hash << 5) + hash) + (uint64_t)str[j];
        }
        if (hash % maxShard == myShard) {
          keys.emplace_back(key);
          vals.emplace_back("1000");
        }
        key = "checkingStore_" + std::to_string(i);
        hash = 5381;
        str = key.c_str();
        for (unsigned int j = 0; j < key.length(); j++) {
          hash = ((hash << 5) + hash) + (uint64_t)str[j];
        }
        if (hash % maxShard == myShard) {
          keys.emplace_back(key);
          vals.emplace_back("50");
        }

        if (keys.size() == 10 || (i == 100000 && keys.size() > 0)) {
          server.Load(keys, vals, Timestamp());
          keys.clear();
          vals.clear();
        }
      }
    } else if (workload.compare("tpcc") == 0) {
      if (keyPath) {
        std::ifstream infile(keyPath);
        std::string line;
        size_t cnt = 0;
        std::vector<std::string> ks, vs;
        while (std::getline(infile, line)) {
            size_t idx = line.find("\t");
            string key = line.substr(0, idx);
            string val = line.substr(idx + 1);
            uint64_t hash = 5381;
            const char* str = key.c_str();
            for (unsigned int j = 0; j < key.length(); j++) {
                hash = ((hash << 5) + hash) + (uint64_t)str[j];
            }

            if (hash % maxShard == myShard) {
                ks.emplace_back(key);
                vs.emplace_back(val);
                ++cnt;
            }

            if (ks.size() >= 200) {
                server.Load(ks, vs, Timestamp());
                ks.clear();
                vs.clear();
            }
        }
        if (ks.size() > 0) {
            server.Load(ks, vs, Timestamp());
        }
      }
    } else {
      std::cout << "No proper workload selected!!!" << std::endl;
    }
    gettimeofday(&t1, NULL);
    double latency = (t1.tv_sec - t0.tv_sec)*1000000 + t1.tv_usec - t0.tv_usec;
    std::cout << "Init took " << (latency/1000000) << " seconds!" << std::endl;

    transport.Run();

    return 0;
}
