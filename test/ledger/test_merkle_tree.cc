#include <string>
#include <vector>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "ledger/common/db.h"
#include "ledger/ledgerdb/merkletree.h"
#include "ledger/ledgerdb/types.h"

TEST(MERKLETREE, UPD) {
  std::vector<std::string> hashes;

  ledgebase::DB db;
  db.Open("testdb");
  ledgebase::ledgerdb::MerkleTree mt(&db);

  uint64_t block_seq = 0;

  timeval t0, t1;
  gettimeofday(&t0, NULL);
  for (size_t i = 0; i < 100; ++i) {
    hashes.emplace_back(ledgebase::Hash::ComputeFrom(std::to_string(i)).ToBase32());
  }

  std::string root_key, root_hash;
  mt.update(block_seq, hashes, "", &root_key, &root_hash);

  for (int i = 0; i < 100; ++i) {
    std::cout << "###### " << i << std::endl;
    auto proof = mt.getProof("0", root_key, 99, i);
    auto res = proof.Verify();
    ASSERT_EQ(true, res);
  }
}

TEST(MERKLETREE, TPS) {
  std::vector<std::string> hashes;

  ledgebase::DB db;
  db.Open("testdb");
  ledgebase::ledgerdb::MerkleTree mt(&db);

  uint64_t block_seq = 0;

  timeval t0, t1;
  gettimeofday(&t0, NULL);
  for (size_t i = 0; i < 100000; ++i) {
    hashes.emplace_back(ledgebase::Hash::ComputeFrom(std::to_string(i)).ToBase32());

    if (hashes.size() == 100) {
      std::string root_key, root_hash;
      mt.update(block_seq, hashes, (block_seq == 0) ?
          "" : std::to_string(block_seq + 1), &root_key, &root_hash);
      ++block_seq;
      hashes.clear();
    }
  }
  gettimeofday(&t1, NULL);
  auto elapsed_time = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
  std::cerr << "# Latency: " << elapsed_time << std::endl;
}