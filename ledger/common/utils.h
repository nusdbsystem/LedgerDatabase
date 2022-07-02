#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <vector>
#include <string>
#include <sstream>

namespace ledgebase {

class Utils {
 public:
  static std::vector<std::string> splitBy(const std::string &str, char delim);
};

}  // namespace ledgebase

#endif