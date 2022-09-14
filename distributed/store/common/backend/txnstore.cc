#include "distributed/store/common/backend/txnstore.h"

using namespace std;

TxnStore::TxnStore() {}
TxnStore::~TxnStore() {}

int TxnStore::BatchGet(uint64_t id, const std::vector<std::string> &keys,      
                       strongstore::proto::Reply* reply) {         
    return 0;                                                                     
}

int TxnStore::GetRange(const std::string &start, const std::string &end,
                       strongstore::proto::Reply* reply) {
    return 0;
}

int
TxnStore::Prepare(uint64_t id, const Transaction &txn)
{
    Panic("Unimplemented PREPARE");
    return 0;
}

void
TxnStore::Abort(uint64_t id, const Transaction &txn)
{
    Panic("Unimplemented ABORT");
}

void
TxnStore::Load(const vector<string> &keys, const vector<string> &values,
               const Timestamp &timestamp)
{
    Panic("Unimplemented LOAD");
}

int TxnStore::GetProof(const std::map<uint64_t, std::vector<std::string>> &keys,
                       strongstore::proto::Reply* reply) {
  return 0;
}

int TxnStore::GetProof(const uint64_t seq,
                       strongstore::proto::Reply* reply) {
  return 0;
}

void
TxnStore::Commit(uint64_t id, uint64_t timestamp,
                 std::vector<std::pair<std::string, size_t>> ver_keys,
                 strongstore::proto::Reply* reply)
{
    Panic("Unimplemented COMMIT");
}

void
TxnStore::Commit(uint64_t id, uint64_t timestamp,
                 strongstore::proto::Reply* reply)
{
    Panic("Unimplemented COMMIT");
}
