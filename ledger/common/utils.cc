#include "ledger/common/utils.h"

namespace ledgebase {

std::vector<std::string> Utils::splitBy(const std::string &str, char delim) {
  std::vector<std::string> vec;
  std::stringstream ss(str);
  while(ss.good()) {
    std::string s;
    getline(ss, s, delim);
    vec.push_back(s);
  }
  return vec;
}

}  // namespace ledgebase