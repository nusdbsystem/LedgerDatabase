#ifndef _VR_REPLICA_H_
#define _VR_REPLICA_H_

#include "distributed/lib/configuration.h"
#include "distributed/replication/common/log.h"
#include "distributed/replication/common/replica.h"
#include "distributed/replication/common/quorumset.h"
#include "distributed/proto/vr-proto.pb.h"

#include <map>
#include <memory>
#include <list>

namespace replication {
namespace vr {

class VRReplica : public Replica
{
public:
    VRReplica(transport::Configuration config, int myIdx,
              Transport *transport, unsigned int batchSize,
              AppReplica *app);
    ~VRReplica();
    
    void ReceiveMessage(const TransportAddress &remote,
                        const string &type, const string &data);

private:
    view_t view;
    opnum_t lastCommitted;
    opnum_t lastOp;
    view_t lastRequestStateTransferView;
    opnum_t lastRequestStateTransferOpnum;
    std::list<std::pair<TransportAddress *,
                        proto::PrepareMessage> > pendingPrepares;
    proto::PrepareMessage lastPrepare;
    unsigned int batchSize;
    opnum_t lastBatchEnd;
    
    Log log;
    std::map<uint64_t, std::unique_ptr<TransportAddress> > clientAddresses;
    struct ClientTableEntry
    {
        uint64_t lastReqId;
        bool replied;
        proto::ReplyMessage reply;
    };
    std::map<uint64_t, ClientTableEntry> clientTable;
    
    QuorumSet<viewstamp_t, proto::PrepareOKMessage> prepareOKQuorum;
    QuorumSet<view_t, proto::StartViewChangeMessage> startViewChangeQuorum;
    QuorumSet<view_t, proto::DoViewChangeMessage> doViewChangeQuorum;

    Timeout *viewChangeTimeout;
    Timeout *nullCommitTimeout;
    Timeout *stateTransferTimeout;
    Timeout *resendPrepareTimeout;
    Timeout *closeBatchTimeout;
    
    bool AmLeader() const;
    void CommitUpTo(opnum_t upto);
    void SendPrepareOKs(opnum_t oldLastOp);
    void RequestStateTransfer();
    void EnterView(view_t newview);
    void StartViewChange(view_t newview);
    void SendNullCommit();
    void UpdateClientTable(const Request &req);
    void ResendPrepare();
    void CloseBatch();
    
    void HandleRequest(const TransportAddress &remote,
                       const proto::RequestMessage &msg);
    void HandleUnloggedRequest(const TransportAddress &remote,
                               const proto::UnloggedRequestMessage &msg);
    
    void HandlePrepare(const TransportAddress &remote,
                       const proto::PrepareMessage &msg);
    void HandlePrepareOK(const TransportAddress &remote,
                         const proto::PrepareOKMessage &msg);
    void HandleCommit(const TransportAddress &remote,
                      const proto::CommitMessage &msg);
    void HandleRequestStateTransfer(const TransportAddress &remote,
                                    const proto::RequestStateTransferMessage &msg);
    void HandleStateTransfer(const TransportAddress &remote,
                             const proto::StateTransferMessage &msg);
    void HandleStartViewChange(const TransportAddress &remote,
                               const proto::StartViewChangeMessage &msg);
    void HandleDoViewChange(const TransportAddress &remote,
                            const proto::DoViewChangeMessage &msg);
    void HandleStartView(const TransportAddress &remote,
                         const proto::StartViewMessage &msg);
};

} // namespace replication::vr
} // namespace replication

#endif  /* _VR_REPLICA_H_ */
