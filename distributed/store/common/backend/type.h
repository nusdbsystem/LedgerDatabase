#ifndef _TYPE_H_
#define _TYPE_H_

#include <string>

struct BKV {
  std::string key;
  uint64_t block;
  std::string val;
};

#endif /* _TYPE_H_ */
