#include "ledger/ledgerdb/skiplist/skiplist.h"

#include <limits>

namespace ledgebase {

namespace ledgerdb {

// Struct SkipNode member implementations
// constructor 
SkipNode::SkipNode (long k, const std::string& v, int level)
    :  key(k), value(v)
{
    for (int i = 0; i < level; ++i) forward.emplace_back(-1);
}

SkipNode::SkipNode (const std::string& node)
{
  auto delim = node.find("|");
  key = std::stol(node.substr(0, delim));
  auto rest = node.substr(delim + 1);
  delim = rest.find("|");
  value = rest.substr(0, delim);
  rest = rest.substr(delim + 1);
  delim = rest.find("|");

  while (delim != std::string::npos) {
    long ptr = std::stol(rest.substr(0, delim));
    forward.emplace_back(ptr);
    rest = rest.substr(delim + 1);
    delim = rest.find("|");
  }
  forward.emplace_back(std::stol(rest));
}

std::string SkipNode::ToString() {
  std::string res = std::to_string(key) + "|" + value;
  for (size_t i = 0; i < forward.size(); ++i) {
    res += "|" + std::to_string(forward[i]);
  }
  return res;
}

// Helper functions
/*
    Function: randomLevel()
    Use: implicit in class Skip_list
    It generates node levels in the range
    [1, maxLevel). 

    It uses rand() scaled by its maximum 
    value: RAND_MAX, so that the randomly 
    generated numbers are within [0,1).
*/
int SkipList::randomLevel () {
    int v = 1;

    while ((((double)std::rand() / RAND_MAX)) < probability && 
           std::abs(v) < maxLevel) {

        v += 1;
    }
    return abs(v);
}

/*
    Function: nodeLevel()
    Use: Implicitly in most of the member functions.

    It returns the number of non-null pointers
    corresponding to the level of the current node.
    (the node that contains the checked vector of 
    forward pointers)

    If list empty returns 1.
*/
int SkipList::nodeLevel (const std::vector<long>& v) {
    int currentLevel = 1;

    if (v[0] == -1) {
        return currentLevel;
    }

    for (size_t i = 1; i < v.size(); ++i) {

        if (v[i] != -1) {
            ++currentLevel;
        } else { 
            break;
        }
    }
    return currentLevel;
}

// Modifying member functions
/*
    Function: makeNode ()
    Use: Implicitly in member function insert().

    It wraps the SkipNode constructor which creates
    a node on the heap and returns a pointer to it. 
*/
SkipNode SkipList::makeNode (int key, std::string val, int level) {
    return SkipNode(key, val, level);
}

/*
    Function: find()
    Use: SkipNode* found = skip_list_obj.find(searchKey);

    It searches the skip list and
    returns the element corresponding 
    to the searchKey; otherwise it returns
    failure, in the form of null pointer.
*/
std::string SkipList::find(const std::string& prefix, long searchKey) {
  std::string headstr;
  db_->Get(prefix + "|head", &headstr);
  if (headstr.size() == 0) {
    return "";
  }
  SkipNode head(headstr);
  if (head.key == searchKey) {
    return headstr;
  }

  unsigned int currentMaximum = nodeLevel(head.forward);
  for (unsigned int i = currentMaximum; i-- > 0;) {
    while (head.forward[i] > -1 && head.forward[i] > searchKey) {
      std::string node;
      db_->Get(prefix + "|" + std::to_string(head.forward[i]), &node);
      head = SkipNode(node);
    }
    if (head.forward[i] == searchKey) {
      std::string node;
      db_->Get(prefix + "|" + std::to_string(head.forward[i]), &node);
      return node;
    }
  }
  return "";
} 

/*
    Function: insert();
    Use: void insert(searchKey, newValue);

    It searches the skip list for elements
    with seachKey, if there is an element
    with that key its value is reassigned to the 
    newValue, otherwise it creates and splices
    a new node, of random level.
*/
void SkipList::insert(const std::string& prefix, long searchKey,
    std::string newValue) {
  // if new skiplist
  std::string headstr;
  db_->Get(prefix + "|head", &headstr);
  if (headstr.size() == 0) {
    SkipNode newhead(searchKey, newValue, maxLevel);
    db_->Put(prefix +"|head", newhead.ToString());
    return;
  }

  // reassign if node exists 
  auto x = find(prefix, searchKey);
  if (x.size() > 0) {
    SkipNode skipnode(x);
    skipnode.value = newValue;
    db_->Put(prefix + "|" + std::to_string(searchKey), skipnode.ToString());
    return;
  }

  std::map<std::string, SkipNode> update;
  SkipNode head(headstr);
  std::string fullkey = prefix + "|head";
  int newNodeLevel = randomLevel();
  auto newnode = SkipNode(searchKey, newValue, newNodeLevel);

  // search the list 
  for (unsigned int i = newNodeLevel; i-- > 0;) {
    while (head.forward[i] > -1 && head.forward[i] > searchKey) {
      fullkey = prefix + "|" + std::to_string(head.forward[i]);
      if (update.find(fullkey) != update.end()) {
        head = update[fullkey];
      } else {
        std::string node;
        db_->Get(fullkey, &node);
        head = SkipNode(node);
      }
    }
    newnode.forward[i] = head.forward[i];
    head.forward[i] = searchKey;
    update[fullkey] = head;
  }

  for (auto& entry : update) {
    db_->Put(entry.first, entry.second.ToString());
  }
  db_->Put(prefix + "|" + std::to_string(searchKey), newnode.ToString());
}

void SkipList::scan(const std::string& prefix, int n,
    std::vector<std::string>& res) {
  std::string nextkey = "head";
  for (int i = 0; i < n; ++i) {
    std::string node;
    db_->Get(prefix + "|" + nextkey, &node);
    if (node.size() == 0) return;
    res.emplace_back(node);
    SkipNode skipnode(node);
    if (skipnode.forward[0] < 0) return;
    nextkey = std::to_string(skipnode.forward[0]);
  }
}

}  // namespace ledgerdb

}  // namespace ledgebase
