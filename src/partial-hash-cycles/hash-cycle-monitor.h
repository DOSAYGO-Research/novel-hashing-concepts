#ifndef HASH_CYCLE_MONITOR_H
#define HASH_CYCLE_MONITOR_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <openssl/evp.h>
#include <algorithm>
#include <stdexcept>
#include <iomanip>

// Custom type definitions
using seed_t = uint32_t;
using HashDigest = std::array<uint8_t, 32>; // 32 bytes for SHA-256

//
// Simple SHA-256 wrapper (with optional byte-swap)
//
template <uint32_t hashsize, bool bswap>
static void hash(const void* in, size_t len, seed_t seed, void* out) {
  if constexpr (hashsize == 32) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
      throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    const EVP_MD* md = EVP_sha256();
    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    // Include seed
    if (EVP_DigestUpdate(ctx, &seed, sizeof(seed)) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestUpdate failed (seed)");
    }

    // Hash input data
    if (EVP_DigestUpdate(ctx, in, len) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestUpdate failed (input)");
    }

    // Finalize
    uint8_t temp_output[32];
    unsigned int output_len = 0;
    if (EVP_DigestFinal_ex(ctx, temp_output, &output_len) != 1 || output_len != hashsize) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    // Optional byte-swap
    if (bswap) {
      for (size_t i = 0; i < hashsize; ++i) {
        reinterpret_cast<uint8_t*>(out)[i] = temp_output[hashsize - 1 - i];
      }
    } else {
      std::memcpy(out, temp_output, hashsize);
    }

    EVP_MD_CTX_free(ctx);
  } else {
    throw std::invalid_argument("Unsupported hash size");
  }
}

//
// A helper hash for std::array<uint8_t, N>
//
namespace std {
  template <typename T, size_t N>
  struct hash<std::array<T, N>> {
    size_t operator()(const std::array<T, N>& arr) const {
      std::hash<T> hasher;
      size_t result = 0;
      for (T val : arr) {
        // Combine in a typical way
        result ^= hasher(val) + 0x9e3779b9 + (result << 6) + (result >> 2);
      }
      return result;
    }
  };
}

//
// The main cycle monitor
//
class HashCycleMonitor {
public:
  // pattern_width, max_length, min_revolutions remain from your original design.
  // For your described "two-step cycle repeated twice," we basically look
  // for an i such that:
  //   P0 is in D_i and D_i+2
  //   P1 is in D_i+1 and D_i+3
  // and then show D_i..D_i+3 in full.
  HashCycleMonitor(uint32_t pattern_width, size_t max_length, size_t min_revolutions)
    : pattern_width_(pattern_width)
    , max_length_(max_length)
    , min_revolutions_(min_revolutions) {
  }

  void enableVerbose(bool verbose) {
    verbose_ = verbose;
  }

  // Main function that drives hashing and detection
  void analyze(const void* input, size_t len, seed_t seed) {
    if (pattern_width_ != 3) {
      throw std::runtime_error("This example is specialized for 3-byte patterns only.");
    }

    for (size_t i = 0; i < max_iterations_; ++i) {
      // Compute current digest
      HashDigest digest;
      hash<32, false>(input, len, seed, digest.data());

      // Store the digest
      all_digests_.push_back(digest);

      // Build a set of all 3-byte patterns found in this digest
      std::unordered_set<std::array<uint8_t, 3>> patternSet;
      extractPatterns(digest, patternSet);

      // Store that set for cycle checks
      iterationPatterns_.push_back(std::move(patternSet));

      // Check if we can detect a two-pattern cycle repeated twice
      // We need at least 4 digests: i, i+1, i+2, i+3
      if (i >= 3) {
        if (checkTwoStepCycle(i - 3)) {
          // Once we find one, we break. If you prefer multiple cycles,
          // remove this break.
          break;
        }
      }

      // Prepare for next iteration
      input = digest.data();
      len = digest.size();
    }
  }

private:
  const uint32_t pattern_width_;
  const size_t max_length_;
  const size_t min_revolutions_;
  bool verbose_ = false;

  // Maximum number of iterations
  static constexpr size_t max_iterations_ = 10000;

  // All digests so we can print them once a cycle is found
  std::vector<HashDigest> all_digests_;

  // For each iteration i, store all 3-byte patterns found in that digest
  std::vector<std::unordered_set<std::array<uint8_t, 3>>> iterationPatterns_;

  //
  // Generate all 3-byte patterns from a digest, sorting each triple so that
  // permutations map to the same "key."
  //
  void extractPatterns(const HashDigest& digest,
                       std::unordered_set<std::array<uint8_t, 3>>& patternSet) {
    for (size_t i = 0; i < digest.size(); ++i) {
      for (size_t j = i + 1; j < digest.size(); ++j) {
        for (size_t k = j + 1; k < digest.size(); ++k) {
          std::array<uint8_t, 3> triple = {
            digest[i], digest[j], digest[k]
          };
          std::sort(triple.begin(), triple.end());
          patternSet.insert(triple);
        }
      }
    }
  }

  //
  // Check for a "2-step cycle repeated twice," specifically:
  //   - Some pattern p0 appears in D_i and D_i+2
  //   - Some pattern p1 appears in D_i+1 and D_i+3
  // We'll do this for the 4 consecutive digests: i, i+1, i+2, i+3.
  //
  bool checkTwoStepCycle(size_t i) {
    // i+3 must be valid
    if (i + 3 >= iterationPatterns_.size()) {
      return false;
    }

    // These sets are from consecutive digests
    const auto& patterns_i   = iterationPatterns_[i];
    const auto& patterns_i1  = iterationPatterns_[i + 1];
    const auto& patterns_i2  = iterationPatterns_[i + 2];
    const auto& patterns_i3  = iterationPatterns_[i + 3];

    // For each candidate p0 in D_i, see if it's also in D_i+2
    // For each candidate p1 in D_i+1, see if it's also in D_i+3
    // If so, we have the cycle described in your example.
    for (const auto& p0 : patterns_i) {
      if (patterns_i2.find(p0) != patterns_i2.end()) {
        // p0 is in D_i and D_i+2
        for (const auto& p1 : patterns_i1) {
          if (patterns_i3.find(p1) != patterns_i3.end()) {
            // p1 is in D_i+1 and D_i+3
            // We found the 2-step repeating cycle: p0 -> p1 -> p0 -> p1
            visualizeCycle(i, p0, p1);
            return true;
          }
        }
      }
    }
    return false;
  }

  //
  // Print out the four consecutive digests:
  //   D_i, D_i+1, D_i+2, D_i+3
  // highlighting p0 in D_i and D_i+2,
  // and p1 in D_i+1 and D_i+3.
  //
  void visualizeCycle(size_t i,
                      const std::array<uint8_t, 3>& p0,
                      const std::array<uint8_t, 3>& p1) const {
    std::cout << "\n--- 2-Step Cycle Repeated Twice Detected ---\n";
    std::cout << "We have:\n";
    std::cout << "  p0 in Digest[" << i << "] and Digest[" << i+2 << "]\n";
    std::cout << "  p1 in Digest[" << i+1 << "] and Digest[" << i+3 << "]\n\n";

    // D_i with p0 highlighted
    highlightDigest(i, p0, /*pLabel=*/"p0");
    // D_i+1 with p1 highlighted
    highlightDigest(i + 1, p1, "p1");
    // D_i+2 with p0 highlighted
    highlightDigest(i + 2, p0, "p0");
    // D_i+3 with p1 highlighted
    highlightDigest(i + 3, p1, "p1");
  }

  //
  // Print one digest, highlighting the bytes in "pattern."
  // pattern is a sorted triple, so if the digest byte is in that triple,
  // we surround it with < > and label it if desired.
  //
  void highlightDigest(size_t digestIndex,
                       const std::array<uint8_t, 3>& pattern,
                       const char* pLabel) const {
    // Retrieve the digest
    if (digestIndex >= all_digests_.size()) {
      return; // Shouldn't happen, but just in case
    }
    const auto& digest = all_digests_[digestIndex];

    std::cout << "Digest[" << digestIndex << "] (" << pLabel << "): ";

    // For convenience, we might put the pattern’s 3 bytes into a small set
    // so we can do fast membership checks
    std::unordered_multiset<uint8_t> patSet(pattern.begin(), pattern.end());

    for (uint8_t b : digest) {
      // Check if b is part of the pattern triple
      auto foundIt = patSet.find(b);
      if (foundIt != patSet.end()) {
        // highlight
        std::cout << "<"
                  << std::hex << std::setw(2) << std::setfill('0') << (int)b
                  << "> ";
        // remove one occurrence from the set, so if the digest has multiple
        // copies of the same byte, we only highlight up to pattern’s count
        patSet.erase(foundIt);
      } else {
        // normal print
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b
                  << " ";
      }
    }
    std::cout << std::dec << "\n\n";
  }
};

#endif // HASH_CYCLE_MONITOR_H

