#ifndef BPLUS_CONFIG_H_
#define BPLUS_CONFIG_H_

#include <string>

namespace ledgebase {

namespace qldb {

class BPlusConfig {
 public:
  static inline void Init(size_t fanout, size_t leaf_fanout) {
    fanout_ = fanout;
    leaf_fanout_ = leaf_fanout;
  }
  static inline size_t fanout() { return fanout_; }
  static inline size_t leaf_fanout() { return leaf_fanout_; }
  static inline size_t ComputeSplitIndex(size_t size) {
      return size / 2 + (size % 2 != 0); }

 private:
  static size_t fanout_;
  static size_t leaf_fanout_;
};

}  // namespace qldb

}  // namespace ledgebase

#endif  // BPLUS_CONFIG_H_
