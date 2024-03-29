#ifndef _LIB_TRANSPORT_H_
#define _LIB_TRANSPORT_H_

#include "distributed/lib/configuration.h"

#include <google/protobuf/message.h>
#include <functional>

#define CLIENT_NETWORK_DELAY 0
#define REPLICA_NETWORK_DELAY 0
#define READ_AT_LEADER 1

class TransportAddress
{
public:
    virtual ~TransportAddress() { }
    virtual TransportAddress *clone() const = 0;
};

class TransportReceiver
{
public:
    typedef ::google::protobuf::Message Message;
    

    virtual ~TransportReceiver();
    virtual void SetAddress(const TransportAddress *addr);
    virtual const TransportAddress& GetAddress();

    virtual void ReceiveMessage(const TransportAddress &remote,
                                const string &type, const string &data) = 0;

    
protected:
    const TransportAddress *myAddress;
};

typedef std::function<void (void)> timer_callback_t;

class Transport
{
protected:
    typedef ::google::protobuf::Message Message;
public:
    virtual ~Transport() {}
    virtual void Register(TransportReceiver *receiver,
                          const transport::Configuration &config,
                          int replicaIdx) = 0;
    virtual bool SendMessage(TransportReceiver *src, const TransportAddress &dst,
                             const Message &m) = 0;
    virtual bool SendMessageToReplica(TransportReceiver *src, int replicaIdx, const Message &m) = 0;
    virtual bool SendMessageToAll(TransportReceiver *src, const Message &m, bool fromSvr) = 0;
    virtual int Timer(uint64_t ms, timer_callback_t cb) = 0;
    virtual bool CancelTimer(int id) = 0;
    virtual void CancelAllTimers() = 0;
};

class Timeout
{
public:
    Timeout(Transport *transport, uint64_t ms, timer_callback_t cb);
    virtual ~Timeout();
    virtual void SetTimeout(uint64_t ms);
    virtual uint64_t Start();
    virtual uint64_t Reset();
    virtual void Stop();
    virtual bool Active() const;
    
private:
    Transport *transport;
    uint64_t ms;
    timer_callback_t cb;
    int timerId;
};

#endif  // _LIB_TRANSPORT_H_
