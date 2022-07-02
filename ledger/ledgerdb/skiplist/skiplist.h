#ifndef LEDGERDB_SKIPLIST_H
#define LEDGERDB_SKIPLIST_H

#include <vector>
#include <string>
#include "ledger/common/db.h"

namespace ledgebase {

namespace ledgerdb {

struct SkipNode {
  long key;
  std::string value;

  // pointers to successor nodes
  std::vector<long> forward;

  SkipNode (long k, const std::string& v, int level);
  SkipNode (const std::string& node);
  SkipNode () = default;
  std::string ToString();
};


class SkipList {
 public:
  SkipList (DB* db) : db_(db), probability(0.5), maxLevel(16) {};
  ~SkipList () = default;

  std::string find (const std::string& prefix, long searchKey);
  void insert (const std::string& prefix, long searchKey,
      std::string newValue);
  void scan(const std::string& prefix, int n, std::vector<std::string>& res);

 private:
  DB* db_;

  // implicitly used member functions
  int randomLevel (); 
  int nodeLevel(const std::vector<long>& v);
  SkipNode makeNode (int key, std::string val, int level);

  // data members  
  float probability;
  int maxLevel;
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // LEDGERDB_SKIPLIST_H
