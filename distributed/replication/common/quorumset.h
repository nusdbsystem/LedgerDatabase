#ifndef _COMMON_QUORUMSET_H_
#define _COMMON_QUORUMSET_H_

namespace replication {

template <class IDTYPE, class MSGTYPE>
class QuorumSet
{
public:
    QuorumSet(int numRequired)
        : numRequired(numRequired)
    {

    }

    void
    Clear()
    {
        messages.clear();
    }

    void
    Clear(IDTYPE vs)
    {
        std::map<int, MSGTYPE> &vsmessages = messages[vs];
        vsmessages.clear();
    }

    int
    NumRequired() const
    {
        return numRequired;
    }

    const std::map<int, MSGTYPE> &
    GetMessages(IDTYPE vs)
    {
        return messages[vs];
    }

    const std::map<int, MSGTYPE> *
    CheckForQuorum(IDTYPE vs)
    {
        std::map<int, MSGTYPE> &vsmessages = messages[vs];
        int count = vsmessages.size();
        if (count >= numRequired) {
            return &vsmessages;
        } else {
            return NULL;
        }
    }

    const std::map<int, MSGTYPE> *
    CheckForQuorum()
    {
        for (const auto &p : messages) {
            const IDTYPE &vs = p.first;
            const std::map<int, MSGTYPE> *quorum = CheckForQuorum(vs);
            if (quorum != nullptr) {
                return quorum;
            }
        }
        return nullptr;
    }

    const std::map<int, MSGTYPE> *
    AddAndCheckForQuorum(IDTYPE vs, int replicaIdx, const MSGTYPE &msg)
    {
        std::map<int, MSGTYPE> &vsmessages = messages[vs];
        if (vsmessages.find(replicaIdx) != vsmessages.end()) {
            // This is a duplicate message

            // But we'll ignore that, replace the old message from
            // this replica, and proceed.
            //
            // XXX Is this the right thing to do? It is for
            // speculative replies in SpecPaxos...
        }

        vsmessages[replicaIdx] = msg;

        return CheckForQuorum(vs);
    }

    void
    Add(IDTYPE vs, int replicaIdx, const MSGTYPE &msg)
    {
        AddAndCheckForQuorum(vs, replicaIdx, msg);
    }

public:
    int numRequired;
private:
    std::map<IDTYPE, std::map<int, MSGTYPE> > messages;
};

}      // namespace replication

#endif  // _COMMON_QUORUMSET_H_
