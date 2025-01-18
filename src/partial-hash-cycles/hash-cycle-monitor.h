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

using seed_t = uint32_t;
using HashDigest = std::array<uint8_t, 32>; // 32 bytes for SHA-256

// Simple SHA-256 wrapper (with optional byte-swap)
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

// Hash for std::array<uint8_t, N>
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

class HashCycleMonitor {
public:
  HashCycleMonitor(uint32_t pattern_width, size_t max_length, size_t min_revolutions)
    : pattern_width_(pattern_width)
    , max_length_(max_length)
    , min_revolutions_(min_revolutions)
  {}

  // Reset the internal state so we can reuse the same object for multiple runs
  void reset() {
    all_digests_.clear();
    iterationPatterns_.clear();
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

      // Gather all 3-byte patterns from this digest
      std::unordered_set<std::array<uint8_t, 3>> patternSet;
      extractPatterns(digest, patternSet);

      // Save that set for cycle checks
      iterationPatterns_.push_back(std::move(patternSet));

      // We only start checking once we have i..i+3
      if (i >= 3) {
        if (checkTwoStepCycle(i - 3)) {
          break; // Found a cycle; stop unless you want multiple cycles
        }
      }

      // Next iteration
      input = digest.data();
      len = digest.size();
    }
  }

private:
  const uint32_t pattern_width_;
  const size_t max_length_;
  const size_t min_revolutions_;
  bool verbose_ = false;

  static constexpr size_t max_iterations_ = 10000;

  std::vector<HashDigest> all_digests_;
  std::vector<std::unordered_set<std::array<uint8_t, 3>>> iterationPatterns_;

  // Extract sorted 3-byte combos from the digest
  void extractPatterns(const HashDigest& digest,
                       std::unordered_set<std::array<uint8_t, 3>>& patternSet) {
    for (size_t i = 0; i < digest.size(); ++i) {
      for (size_t j = i + 1; j < digest.size(); ++j) {
        for (size_t k = j + 1; k < digest.size(); ++k) {
          std::array<uint8_t, 3> triple = { digest[i], digest[j], digest[k] };
          std::sort(triple.begin(), triple.end());
          patternSet.insert(triple);
        }
      }
    }
  }

  // Check for the "2-step cycle repeated twice":
  //   p0 in D_i & D_i+2
  //   p1 in D_i+1 & D_i+3
  bool checkTwoStepCycle(size_t i) {
    if (i + 3 >= iterationPatterns_.size()) {
      return false;
    }

    const auto& pat_i   = iterationPatterns_[i];
    const auto& pat_i1  = iterationPatterns_[i + 1];
    const auto& pat_i2  = iterationPatterns_[i + 2];
    const auto& pat_i3  = iterationPatterns_[i + 3];

    for (const auto& p0 : pat_i) {
      if (pat_i2.find(p0) != pat_i2.end()) {
        for (const auto& p1 : pat_i1) {
          if (pat_i3.find(p1) != pat_i3.end()) {
            // Found the cycle
            visualizeCycle(i, p0, p1);
            return true;
          }
        }
      }
    }
    return false;
  }

  // Show the four consecutive digests [i..i+3], highlighting p0 in D_i,D_i+2
  // and p1 in D_i+1,D_i+3, along with a simple cycle line: p0 -> p1 -> p0
  void visualizeCycle(size_t i,
                      const std::array<uint8_t, 3>& p0,
                      const std::array<uint8_t, 3>& p1) const {
    std::cout << "\n--- 2-Step Cycle Repeated Twice Detected ---\n"
              << "We have:\n"
              << "  p0 in Digest[" << i << "] and Digest[" << (i+2) << "]\n"
              << "  p1 in Digest[" << (i+1) << "] and Digest[" << (i+3) << "]\n\n";

    // Show the line: p0 -> p1 -> p0 in hex
    std::cout << "Cycle: (p0) ";
    printTripleHex(p0);
    std::cout << " -> (p1) ";
    printTripleHex(p1);
    std::cout << " -> (p0) ";
    printTripleHex(p0);
    std::cout << "\n\n";

    highlightDigest(i,     p0, "p0");
    highlightDigest(i + 1, p1, "p1");
    highlightDigest(i + 2, p0, "p0");
    highlightDigest(i + 3, p1, "p1");
  }

  // Helper to print a sorted 3-byte triple
  void printTripleHex(const std::array<uint8_t, 3>& triple) const {
    std::cout << "[ ";
    for (auto b : triple) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)b << " ";
    }
    std::cout << std::dec << "]";
  }

  // Print one digest, highlighting bytes in "pattern" with < >
  void highlightDigest(size_t digestIndex,
                       const std::array<uint8_t, 3>& pattern,
                       const char* pLabel) const {
    if (digestIndex >= all_digests_.size()) return;

    const auto& digest = all_digests_[digestIndex];
    std::unordered_multiset<uint8_t> patSet(pattern.begin(), pattern.end());

    std::cout << "Digest[" << digestIndex << "] (" << pLabel << "): ";
    for (uint8_t b : digest) {
      auto it = patSet.find(b);
      if (it != patSet.end()) {
        // highlight
        std::cout << "<"
                  << std::hex << std::setw(2) << std::setfill('0') << (int)b
                  << "> ";
        patSet.erase(it); // remove one occurrence
      } else {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b
                  << " ";
      }
    }
    std::cout << std::dec << "\n\n";
  }
};

#endif // HASH_CYCLE_MONITOR_H

