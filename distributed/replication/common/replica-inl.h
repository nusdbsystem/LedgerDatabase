#ifndef _COMMON_REPLICA_INL_H_
#define _COMMON_REPLICA_INL_H_

template<class MSG>
void
Replica::Execute(opnum_t opnum,
                 const Request &msg,
                 MSG &reply)
{
    string res;
    ReplicaUpcall(opnum, msg.op(), res);

    reply.set_reply(res);
}

template<class MSG>
void
Replica::ExecuteUnlogged(const UnloggedRequest &msg,
                           MSG &reply)
{
    string res;
    UnloggedUpcall(msg.op(), res);

    reply.set_reply(res);
}

#endif // _COMMON_REPLICA_INL_H_
