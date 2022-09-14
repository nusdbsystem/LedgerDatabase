#include "distributed/replication/common/client.h"
#include "distributed/proto/request.pb.h"
#include "distributed/lib/message.h"
#include "distributed/lib/transport.h"

#include <random>

namespace replication {

std::string ErrorCodeToString(ErrorCode err) {
    switch (err) {
        case ErrorCode::TIMEOUT:
            return "TIMEOUT";
        case ErrorCode::MISMATCHED_CONSENSUS_VIEWS:
            return "MISMATCHED_CONSENSUS_VIEWS";
        default:
            Assert(false);
            return "";
    }
}

Client::Client(const transport::Configuration &config, Transport *transport,
               uint64_t clientid)
    : config(config), transport(transport)
{
    this->clientid = clientid;

    // Randomly generate a client ID
    // This is surely not the fastest way to get a random 64-bit int,
    // but it should be fine for this purpose.
    while (this->clientid == 0) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
        this->clientid = dis(gen);
            }

    transport->Register(this, config, -1);
}

Client::~Client()
{

}

void
Client::ReceiveMessage(const TransportAddress &remote,
                       const string &type, const string &data)
{
    Panic("Received unexpected message type: %s",
          type.c_str());
}

} // namespace replication
