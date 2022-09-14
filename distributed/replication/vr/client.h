#ifndef _VR_CLIENT_H_
#define _VR_CLIENT_H_

#include "distributed/replication/common/client.h"
#include "distributed/lib/configuration.h"
#include "distributed/proto/vr-proto.pb.h"

#include <unordered_map>

namespace replication {
namespace vr {

class VRClient : public Client
{
public:
    VRClient(const transport::Configuration &config,
             Transport *transport,
             uint64_t clientid = 0);
    virtual ~VRClient();
    virtual void Invoke(const string &request,
                        continuation_t continuation,
                        error_continuation_t error_continuation = nullptr);
    virtual void InvokeUnlogged(int replicaIdx,
                                const string &request,
                                continuation_t continuation,
                                error_continuation_t error_continuation = nullptr,
                                uint32_t timeout = DEFAULT_UNLOGGED_OP_TIMEOUT);
    virtual void ReceiveMessage(const TransportAddress &remote,
                                const string &type, const string &data);

protected:
    int view;
    int opnumber;
    uint64_t lastReqId;

    struct PendingRequest
    {
        string request;
        uint64_t clientReqId;
        continuation_t continuation;
	Timeout *timer;
        inline PendingRequest(string request,
			      uint64_t clientReqId,
                              continuation_t continuation,
			      Timeout *timer)
            : request(request), clientReqId(clientReqId),
              continuation(continuation), timer(timer) { };
	inline ~PendingRequest() { delete timer; }
    };

    struct PendingUnloggedRequest : public PendingRequest
    {
	error_continuation_t error_continuation;
        inline PendingUnloggedRequest(string request,
				      uint64_t clientReqId,
				      continuation_t continuation,
				      Timeout *timer,
				      error_continuation_t error_continuation)
            : PendingRequest(request, clientReqId, continuation, timer),
              error_continuation(error_continuation) { };
    };

    std::unordered_map<uint64_t, PendingRequest *> pendingReqs;

    void SendRequest(const PendingRequest *req);
    void ResendRequest(const uint64_t reqId);
    void HandleReply(const TransportAddress &remote,
                     const proto::ReplyMessage &msg);
    void HandleUnloggedReply(const TransportAddress &remote,
                             const proto::UnloggedReplyMessage &msg);
    void UnloggedRequestTimeoutCallback(const uint64_t reqId);
};

} // namespace replication::vr
} // namespace replication

#endif  /* _VR_CLIENT_H_ */
