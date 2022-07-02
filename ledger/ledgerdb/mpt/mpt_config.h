#ifndef MPT_CONFIG_H_
#define MPT_CONFIG_H_

#include <string>

namespace ledgebase {

namespace ledgerdb {

class MPTConfig {
 public:
  static std::string KeybytesToHex(const std::string& key) {
    size_t l = key.length() * 2 + 1;
    std::string nibbles;
    nibbles.resize(l);
    for (size_t i = 0; i < key.length(); ++i) {
      nibbles[i*2] = *reinterpret_cast<const int8_t*>(key.c_str() + i) / 16;
      nibbles[i*2+1] = *reinterpret_cast<const int8_t*>(key.c_str() + i) % 16;
    }
    nibbles[l-1] = 16;
    return nibbles;
  }

  static size_t PrefixLen(const std::string& key1, const std::string& key2) {
    size_t i = 0;
    while (i < key1.size() && i < key2.size() && key1[i] == key2[i]) {
      ++i;
    }
    return i;
  }
};

}  // namespace ledgerdb

}  // namespace ledgebase

#endif  // MPT_CONFIG_H_
