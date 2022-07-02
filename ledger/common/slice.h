#ifndef SLICE_H_
#define SLICE_H_

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

namespace ledgebase {

typedef unsigned char byte_t;

class Slice {
 public:
  Slice() = default;
  Slice(const Slice&) = default;
  Slice(const byte_t* slice, size_t len) : data_(slice), len_(len) {}
  // share data from c string
  Slice(const char* slice, size_t len)
    : Slice(reinterpret_cast<const byte_t*>(slice), len) {}
  explicit Slice(const char* slice) : Slice(slice, std::strlen(slice)) {}
  // share data from c++ string
  explicit Slice(const std::string& slice)
    : Slice(slice.data(), slice.length()) {}
  // delete constructor that takes in rvalue std::string
  //   to avoid the memory space of parameter is released unawares.
  explicit Slice(std::string&&) = delete;
  ~Slice() = default;

  Slice& operator=(const Slice&) = default;

  inline bool operator<(const Slice& slice) const {
    size_t min_len = std::min(len_, slice.len_);
    int cmp = std::memcmp(data_, slice.data_, min_len);
    return cmp ? cmp < 0 : len_ < slice.len_;
  }
  inline bool operator>(const Slice& slice) const {
    size_t min_len = std::min(len_, slice.len_);
    int cmp = std::memcmp(data_, slice.data_, min_len);
    return cmp ? cmp > 0 : len_ > slice.len_;
  }
  inline bool operator==(const Slice& slice) const {
    if (len_ != slice.len_) return false;
    return std::memcmp(data_, slice.data_, len_) == 0;
  }
  inline bool operator!=(const Slice& slice) const {
    return !operator==(slice);
  }
  inline bool operator==(const std::string& str) const {
    if (len_ != str.size()) return false;
    return std::memcmp(data_, str.c_str(), len_) == 0;
  }
  inline bool operator!=(const std::string& str) const {
    return !operator==(str);
  }
  friend inline bool operator==(const std::string& str, const Slice& slice) {
    return slice == str;
  }
  friend inline bool operator!=(const std::string& str, const Slice& slice) {
    return str != slice;
  }

  inline bool empty() const { return len_ == 0; }
  inline size_t len() const { return len_; }
  inline const byte_t* data() const { return data_; }

  inline std::string ToString() const {
    return std::string(reinterpret_cast<const char*>(data_), len_);
  }

  friend inline std::ostream& operator<<(std::ostream& os, const Slice& obj) {
    // avoid memory copy here
    // os << obj.ToString();
    const char* ptr = reinterpret_cast<const char*>(obj.data_);
    for (size_t i = 0; i < obj.len_; ++i) os << *(ptr++);
    return os;
  }

 private:
  const byte_t* data_ = nullptr;
  size_t len_ = 0;
};

}  // namespace ledgebase

#endif  // SLICE_H_
