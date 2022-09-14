#ifndef _STRONG_SERVER_H_
#define _STRONG_SERVER_H_

#include <vector>
#include "distributed/lib/tcptransport.h"
#include "distributed/replication/vr/replica.h"
#include "distributed/store/common/truetime.h"
#include "distributed/store/strongstore/occstore.h"
#include "distributed/proto/strong-proto.pb.h"

namespace strongstore {

enum Mode {
    MODE_UNKNOWN,
    MODE_OCC,
    MODE_LOCK,
    MODE_SPAN_OCC,
    MODE_SPAN_LOCK
};

class Server : public replication::AppReplica
{
public:
    Server(Mode mode, uint64_t skew, uint64_t error, int index, bool sp,
        int timeout);
    virtual ~Server();

    virtual void LeaderUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2);
    virtual void ReplicaUpcall(opnum_t opnum, const string &str1, string &str2);
    virtual void UnloggedUpcall(const string &str1, string &str2);
    void Load(const string &key, const string &value, const Timestamp timestamp);
    void Load(const std::vector<std::string> &keys,
              const std::vector<std::string> &values,
              const Timestamp timestamp);

private:
    Mode mode;
    bool stored_procedure;
    TxnStore *store;
    TrueTime timeServer;
};

} // namespace strongstore

#endif /* _STRONG_SERVER_H_ */
