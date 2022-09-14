#ifndef _KV_STORE_H_
#define _KV_STORE_H_

#include "distributed/lib/assert.h"
#include "distributed/lib/message.h"
#include <string>
#include <unordered_map>
#include <list>

class KVStore
{

public:
    KVStore();
    ~KVStore();

    bool get(const std::string &key, std::string &value);
    bool put(const std::string &key, const std::string &value);
    bool remove(const std::string &key, std::string &value);

private:
    /* Global store which keeps key -> (timestamp, value) list. */
    std::unordered_map<std::string, std::list<std::string>> store;
};

#endif  /* _KV_STORE_H_ */
