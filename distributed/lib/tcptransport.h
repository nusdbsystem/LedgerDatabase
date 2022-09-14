#ifndef _LIB_TCPTRANSPORT_H_
#define _LIB_TCPTRANSPORT_H_

#include "distributed/lib/configuration.h"
#include "distributed/lib/transport.h"
#include "distributed/lib/transportcommon.h"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <map>
#include <unordered_map>
#include <list>
#include <random>
#include <mutex>
#include <netinet/in.h>

class TCPTransportAddress : public TransportAddress
{
public:
    TCPTransportAddress * clone() const;
private:
    TCPTransportAddress(const sockaddr_in &addr);
    
    sockaddr_in addr;
    friend class TCPTransport;
    friend bool operator==(const TCPTransportAddress &a,
                           const TCPTransportAddress &b);
    friend bool operator!=(const TCPTransportAddress &a,
                           const TCPTransportAddress &b);
    friend bool operator<(const TCPTransportAddress &a,
                          const TCPTransportAddress &b);
};

class TCPTransport : public TransportCommon<TCPTransportAddress>
{
public:
    TCPTransport(double dropRate = 0.0, double reogrderRate = 0.0,
                    int dscp = 0, bool handleSignals = true);
    virtual ~TCPTransport();
    void Register(TransportReceiver *receiver,
                  const transport::Configuration &config,
                  int replicaIdx);
    void Run();
    void Stop();
    int Timer(uint64_t ms, timer_callback_t cb);
    bool CancelTimer(int id);
    void CancelAllTimers();
    
private:
    std::mutex mtx;
    struct TCPTransportTimerInfo
    {
        TCPTransport *transport;
        timer_callback_t cb;
        event *ev;
        int id;
    };
    struct TCPTransportTCPListener
    {
        TCPTransport *transport;
        TransportReceiver *receiver;
        int acceptFd;
        int replicaIdx;
        event *acceptEvent;
        std::list<struct bufferevent *> connectionEvents;
    };
    event_base *libeventBase;
    std::vector<event *> listenerEvents;
    std::vector<event *> signalEvents;
    std::map<int, TransportReceiver*> receivers; // fd -> receiver
    std::map<TransportReceiver*, int> fds; // receiver -> fd
    int lastTimerId;
    std::map<int, TCPTransportTimerInfo *> timers;
    std::list<TCPTransportTCPListener *> tcpListeners;
    std::map<TCPTransportAddress, struct bufferevent *> tcpOutgoing;
    std::map<struct bufferevent *, TCPTransportAddress> tcpAddresses;
    
    bool SendMessageInternal(TransportReceiver *src,
                             const TCPTransportAddress &dst,
                             const Message &m, bool multicast = false);

    TCPTransportAddress
    LookupAddress(const transport::ReplicaAddress &addr);
    TCPTransportAddress
    LookupAddress(const transport::Configuration &cfg,
                  int replicaIdx);
    const TCPTransportAddress *
    LookupMulticastAddress(const transport::Configuration*config) { return NULL; };

    void ConnectTCP(TransportReceiver *src, const TCPTransportAddress &dst);
    void OnTimer(TCPTransportTimerInfo *info);
    static void TimerCallback(evutil_socket_t fd,
                              short what, void *arg);
    static void LogCallback(int severity, const char *msg);
    static void FatalCallback(int err);
    static void SignalCallback(evutil_socket_t fd,
                               short what, void *arg);
    static void TCPAcceptCallback(evutil_socket_t fd, short what,
                                  void *arg);
    static void TCPReadableCallback(struct bufferevent *bev,
                                    void *arg);
    static void TCPEventCallback(struct bufferevent *bev,
                                 short what, void *arg);
    static void TCPIncomingEventCallback(struct bufferevent *bev,
                                         short what, void *arg);
    static void TCPOutgoingEventCallback(struct bufferevent *bev,
                                         short what, void *arg);
};

#endif  // _LIB_TCPTRANSPORT_H_
