#ifndef _COMMON_REPLICA_H_
#define _COMMON_REPLICA_H_


#include "distributed/lib/configuration.h"
#include "distributed/replication/common/log.h"
#include "distributed/proto/request.pb.h"
#include "distributed/lib/transport.h"
#include "distributed/replication/common/viewstamp.h"

namespace replication {
    
class Replica;

enum ReplicaStatus {
    STATUS_NORMAL,
    STATUS_VIEW_CHANGE,
    STATUS_RECOVERING
};

class AppReplica
{
public:
    AppReplica() { };
    virtual ~AppReplica() { };
    // Invoke callback on the leader, with the option to replicate on success 
    virtual void LeaderUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2) { replicate = true; str2 = str1; };
    // Invoke callback on all replicas
    virtual void ReplicaUpcall(opnum_t opnum, const string &str1, string &str2) { };
    // Invoke call back for unreplicated operations run on only one replica
    virtual void UnloggedUpcall(const string &str1, string &str2) { };
};

class Replica : public TransportReceiver
{
public:
    Replica(const transport::Configuration &config, int myIdx, Transport *transport, AppReplica *app);
    virtual ~Replica();
    
protected:
    void LeaderUpcall(opnum_t opnum, const string &op, bool &replicate, string &res);
    void ReplicaUpcall(opnum_t opnum, const string &op, string &res);
    template<class MSG> void Execute(opnum_t opnum,
                                     const Request & msg,
                                     MSG &reply);
    void UnloggedUpcall(const string &op, string &res);
    template<class MSG> void ExecuteUnlogged(const UnloggedRequest & msg,
                                               MSG &reply);
    
protected:
    transport::Configuration configuration;
    int myIdx;
    Transport *transport;
    AppReplica *app;
    ReplicaStatus status;
};
    
#include "replica-inl.h"

} // namespace replication

#endif  /* _COMMON_REPLICA_H */
