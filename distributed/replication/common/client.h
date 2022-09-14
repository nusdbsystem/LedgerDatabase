#ifndef _COMMON_CLIENT_H_
#define _COMMON_CLIENT_H_

#include "distributed/lib/configuration.h"
#include "distributed/proto/request.pb.h"
#include "distributed/lib/transport.h"


#include <functional>

namespace replication {

// A client's request may fail for various reasons. For example, if enough
// replicas are down, a client's request may time out. An ErrorCode indicates
// the reason that a client's request failed.
enum class ErrorCode {
    // For whatever reason (failed replicas, slow network), the request took
    // too long and timed out.
    TIMEOUT,

    // For IR, if a client issues a consensus operation and receives a majority
    // of replies and confirms in different views, then the operation fails.
    MISMATCHED_CONSENSUS_VIEWS
};

std::string ErrorCodeToString(ErrorCode err);

class Client : public TransportReceiver
{
public:
    using continuation_t =
        std::function<void(const string &request, const string &reply)>;
    using error_continuation_t =
        std::function<void(const string &request, ErrorCode err)>;

    static const uint32_t DEFAULT_UNLOGGED_OP_TIMEOUT = 1000; // milliseconds

    Client(const transport::Configuration &config, Transport *transport,
           uint64_t clientid = 0);
    virtual ~Client();

    virtual void Invoke(
        const string &request,
        continuation_t continuation,
        error_continuation_t error_continuation = nullptr) = 0;
    virtual void InvokeUnlogged(
        int replicaIdx,
        const string &request,
        continuation_t continuation,
        error_continuation_t error_continuation = nullptr,
        uint32_t timeout = DEFAULT_UNLOGGED_OP_TIMEOUT) = 0;

    virtual void ReceiveMessage(const TransportAddress &remote,
                                const string &type,
                                const string &data);

protected:
    transport::Configuration config;
    Transport *transport;

    uint64_t clientid;
};

} // namespace replication

#endif  /* _COMMON_CLIENT_H_ */
