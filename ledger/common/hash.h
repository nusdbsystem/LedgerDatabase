#ifndef HASH_H
#define HASH_H

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace ledgebase {

class Hash {
 public:
  static constexpr size_t kByteLength = 20;
  static constexpr size_t kBase32Length = 32;
  static const Hash kNull;
  static Hash FromBase32(const std::string& base32);
  static Hash ComputeFrom(const unsigned char* data, size_t len);
  static Hash ComputeFrom(const std::string& data);

  Hash() = default;
  // movable
  Hash(Hash&&) = default;
  // use existing hash
  Hash(const Hash& hash) noexcept : value_(hash.value_) {}
  // use existing byte array
  explicit Hash(const unsigned char* hash) noexcept : value_(hash) {}
  // use existing string
  explicit Hash(const std::string& str) noexcept
    : value_(reinterpret_cast<const unsigned char*>(str.data())) {}
  ~Hash() = default;

  // copy and move assignment
  Hash& operator=(Hash hash) noexcept {
    own_.swap(hash.own_);
    std::swap(value_, hash.value_);
    return *this;
  }

  friend inline bool operator==(const Hash& lhs, const Hash& rhs) noexcept {
    if (!lhs.value_ || !rhs.value_) return lhs.value_ == rhs.value_;
    return std::memcmp(lhs.value_, rhs.value_, Hash::kByteLength) == 0;
  }

  friend inline bool operator!=(const Hash& lhs, const Hash& rhs) noexcept {
    return !operator==(lhs, rhs);
  }

  friend inline bool operator<(const Hash& lhs, const Hash& rhs) noexcept {        
    if (!lhs.value_ || !rhs.value_) return lhs.value_ < rhs.value_;             
    return std::memcmp(lhs.value_, rhs.value_, Hash::kByteLength) < 0;          
  }                                                                             
                                                                                
  friend inline bool operator>(const Hash& lhs, const Hash& rhs) noexcept {        
    if (!lhs.value_ || !rhs.value_) return lhs.value_ > rhs.value_;             
    return std::memcmp(lhs.value_, rhs.value_, Hash::kByteLength) > 0;          
  }

  friend inline bool operator<=(const Hash& lhs, const Hash& rhs) noexcept {    
    return !operator>(lhs, rhs);                                                
  }                                                                             
                                                                                
  friend inline bool operator>=(const Hash& lhs, const Hash& rhs) noexcept {    
    return !operator<(lhs, rhs);                                                
  }

  // check if the hash is empty
  inline bool empty() const { return value_ == nullptr; }
  // check if hash owns the data
  inline bool own() const { return own_.get() != nullptr; }
  // expose byte array to others
  inline const unsigned char* value() const { return value_; }
  // get a copy that contains own bytes
  Hash Clone() const;
  // convert to base32 string
  std::string ToBase32() const;
  // get a string version copy
  inline std::string ToString() const {
    return std::string(reinterpret_cast<const char*>(value_),
                       Hash::kByteLength);
  }

  friend inline std::ostream& operator<<(std::ostream& os, const Hash& obj) {
    os << (obj == kNull ? "<null>" : obj.ToString());
    return os;
  }

 private:
  static const unsigned char kEmptyBytes[kByteLength];

  // allocate space if previously not have
  void Alloc();

  // hash own the value if is calculated by itself
  std::unique_ptr<unsigned char[]> own_;
  // otherwise the pointer should be read-only
  // big-endian
  const unsigned char* value_ = nullptr;
};

}

// namespace std {
// template<>
// struct hash<::ledgebase::Hash> {
//   inline size_t operator()(const ::ledgebase::Hash& obj) const {
//     const auto& hval = obj.value();
//     size_t ret;
//     std::copy(hval, hval + sizeof(ret),
//               reinterpret_cast<unsigned char*>(&ret));
//     return ret;
//   }
// };
// }  // namespace std

#endif  // HASH_H
