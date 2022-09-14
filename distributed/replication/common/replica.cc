#include "distributed/replication/common/log.h"
#include "distributed/replication/common/replica.h"

#include "distributed/lib/message.h"

#include <stdlib.h>

namespace replication {
    
Replica::Replica(const transport::Configuration &configuration, int myIdx,
                 Transport *transport, AppReplica *app)
    : configuration(configuration), myIdx(myIdx),
      transport(transport), app(app)
{
    transport->Register(this, configuration, myIdx);
}

Replica::~Replica()
{
    
}

void
Replica::LeaderUpcall(opnum_t opnum, const string &op, bool &replicate, string &res)
{
        app->LeaderUpcall(opnum, op, replicate, res);
    }

void
Replica::ReplicaUpcall(opnum_t opnum, const string &op, string &res)
{
        app->ReplicaUpcall(opnum, op, res);
    
    }

void
Replica::UnloggedUpcall(const string &op, string &res)
{
    app->UnloggedUpcall(op, res);
}

} // namespace replication
