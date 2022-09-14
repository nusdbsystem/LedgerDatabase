#ifndef _LIB_CONFIGURATION_H_
#define _LIB_CONFIGURATION_H_

#include "distributed/replication/common/viewstamp.h"

#include <fstream>
#include <stdbool.h>
#include <string>
#include <vector>

using std::string;

namespace transport {

struct ReplicaAddress
{
    string host;
    string port;
    ReplicaAddress(const string &host, const string &port);
    bool operator==(const ReplicaAddress &other) const;
    inline bool operator!=(const ReplicaAddress &other) const {
        return !(*this == other);
    }
    bool operator<(const ReplicaAddress &other) const;
    bool operator<=(const ReplicaAddress &other) const {
        return *this < other || *this == other;
    }
    bool operator>(const ReplicaAddress &other) const {
        return !(*this <= other);
    }
    bool operator>=(const ReplicaAddress &other) const {
        return !(*this < other);
    }
};


class Configuration
{
public:
    Configuration(const Configuration &c);
    Configuration(int n, int f, std::vector<ReplicaAddress> replicas,
                  ReplicaAddress *multicastAddress = nullptr);
    Configuration(std::ifstream &file);
    virtual ~Configuration();
    ReplicaAddress replica(int idx) const;
    const ReplicaAddress *multicast() const;
    int GetLeaderIndex(view_t view) const;
    int QuorumSize() const;
    int FastQuorumSize() const;
    bool operator==(const Configuration &other) const;
    inline bool operator!=(const Configuration &other) const {
        return !(*this == other);
    }
    bool operator<(const Configuration &other) const;
    bool operator<=(const Configuration &other) const {
        return *this < other || *this == other;
    }
    bool operator>(const Configuration &other) const {
        return !(*this <= other);
    }
    bool operator>=(const Configuration &other) const {
        return !(*this < other);
    }

public:
    int n;                      // number of replicas
    int f;                      // number of failures tolerated
private:
    std::vector<ReplicaAddress> replicas;
    ReplicaAddress *multicastAddress;
    bool hasMulticast;
};

}      // namespace transport


namespace std {
template <> struct hash<transport::ReplicaAddress>
{
    size_t operator()(const transport::ReplicaAddress & x) const
        {
            return hash<string>()(x.host) * 37 + hash<string>()(x.port);
        }
};
}

namespace std {
template <> struct hash<transport::Configuration>
{
    size_t operator()(const transport::Configuration & x) const
        {
            size_t out = 0;
            out = x.n * 37 + x.f;
            for (int i = 0; i < x.n; i++) {
                out *= 37;
                out += hash<transport::ReplicaAddress>()(x.replica(i));
            }
            return out;
        }
};
}

#endif  /* _LIB_CONFIGURATION_H_ */
