#include "ledger/common/hash.h"

#include <cstring>
#include <iostream>
#include <unordered_map>
#include <utility>
#include "cryptopp/cryptlib.h"
#include "cryptopp/sha.h"
#include "cryptopp/blake2.h"

namespace ledgebase {

const unsigned char Hash::kEmptyBytes[] = {};
const Hash Hash::kNull(kEmptyBytes);

std::string test = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr char base32alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

Hash Hash::FromBase32(const std::string& base32) {
  Hash h;
  h.Alloc();
  uint64_t tmp;
  size_t dest = 0;
  for (size_t i = 0; i < kBase32Length; i += 8) {
    tmp = 0;
    for (size_t j = 0; j < 8; ++j) {
      tmp <<= 5;
      auto idx = test.find(base32[i+j]);
      tmp += idx;
      // tmp += (base32[i + j] - 'A');
    }
    for (size_t j = 0; j < 5; ++j) {
      h.own_[dest + 4 - j] = (unsigned char) (tmp & ((1 << 8) - 1));
      tmp >>= 8;
    }
    dest += 5;
  }
  return h;
}

std::string Hash::ToBase32() const {
  if (empty()) {
    return std::string();
  }
  std::string ret;
  uint64_t tmp = 0;
  for (size_t i = 0; i < kByteLength; i += 5) {
    tmp = 0;
    for (size_t j = 0; j < 5; ++j) tmp = (tmp << 8) + uint64_t(value_[i + j]);
    for (size_t j = 0; j < 8; ++j) {
      ret += base32alphabet[size_t(uint8_t(tmp >> 5 * (7 - j)))];
      tmp &= (uint64_t(1) << 5 * (7 - j)) - 1;
    }
  }
  return ret;
}

Hash Hash::Clone() const {
  Hash hash;
  if (!empty()) {
    hash.Alloc();
    std::memcpy(hash.own_.get(), value_, kByteLength);
  }
  return hash;
}

void Hash::Alloc() {
  if (!own_) {
    own_.reset(new unsigned char[kByteLength]);
    value_ = own_.get();
  }
}

Hash Hash::ComputeFrom(const unsigned char* data, size_t len) {
  unsigned char fullhash[CryptoPP::BLAKE2b::DIGESTSIZE];
  Hash h;
  h.Alloc();
  CryptoPP::BLAKE2b hash_gen;
  hash_gen.CalculateDigest(fullhash, data, len);
  std::copy(fullhash, fullhash + kByteLength, h.own_.get());
  return h;
  //unsigned char fullhash[CryptoPP::SHA256::DIGESTSIZE];
  //Hash h;
  //h.Alloc();
  //CryptoPP::SHA256 hash_gen;
  //hash_gen.CalculateDigest(fullhash, data, len);
  //std::copy(fullhash, fullhash + kByteLength, h.own_.get());
  //return h;
}

Hash Hash::ComputeFrom(const std::string& data) {
  return Hash::ComputeFrom(reinterpret_cast<const unsigned char*>(data.c_str()),
                           data.size());
}

}  // namespace ledgebase
