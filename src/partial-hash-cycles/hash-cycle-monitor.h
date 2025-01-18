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
#include <random>

// ---------- Type Definitions ----------
using seed_t = uint32_t;
using HashDigest = std::array<uint8_t, 32>; // 32 bytes for SHA-256

// ---------- Hash Function for std::array<uint8_t, N> ----------
namespace std {
  template <typename T, size_t N>
  struct hash<std::array<T, N>> {
    size_t operator()(const std::array<T, N>& arr) const {
      std::hash<T> hasher;
      size_t result = 0;
      for (T val : arr) {
        // Combine hashed elements in a typical way
        result ^= hasher(val) + 0x9e3779b9 + (result << 6) + (result >> 2);
      }
      return result;
    }
  };
}

// ---------- SHA-256 Wrapper (with optional byte-swap) ----------
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

// ---------- HashCycleMonitor Class ----------
class HashCycleMonitor {
public:
  HashCycleMonitor(uint32_t pattern_width, size_t max_length, size_t min_revolutions)
    : pattern_width_(pattern_width)
    , max_length_(max_length)
    , min_revolutions_(min_revolutions) {}

  // Toggle random benchmark mode (instead of iterative hashing)
  void enableBenchmarkMode(bool mode) {
    benchmarkMode_ = mode;
  }

  // Clear stored digests/patterns so the same object can be reused
  void reset() {
    all_digests_.clear();
    iterationPatterns_.clear();
  }

  // Turn verbose output on/off
  void enableVerbose(bool verbose) {
    verbose_ = verbose;
  }

  // Main driver for the hashing or random generation
  void analyze(const void* input, size_t len, seed_t seed) {
    if (pattern_width_ != 3) {
      throw std::runtime_error("This example is specialized for 3-byte patterns only.");
    }

    for (size_t i = 0; i < max_iterations_; ++i) {
      HashDigest digest;

      if (benchmarkMode_) {
        // Generate a random 32-byte digest
        generateRandomDigest(digest);
      } else {
        // Compute hash digest from previous iteration
        hash<32, false>(input, len, seed, digest.data());
      }

      // Store the newly created or hashed digest
      all_digests_.push_back(digest);

      // Extract 3-byte patterns from this digest
      std::unordered_set<std::array<uint8_t, 3>> patternSet;
      extractPatterns(digest, patternSet);
      iterationPatterns_.push_back(std::move(patternSet));

      // Check for 2-step cycle once we have at least 4 digests
      if (i >= 3) {
        if (checkTwoStepCycle(i - 3)) {
          break; // We found a cycle, so stop unless we want to find more
        }
      }

      // For the next iteration, feed the digest back in if we're in hash mode
      if (!benchmarkMode_) {
        input = digest.data();
        len   = digest.size();
      }
    }
  }

private:
  // ----- Configuration Parameters -----
  const uint32_t pattern_width_;
  const size_t   max_length_;
  const size_t   min_revolutions_;
  bool           verbose_       = false;
  bool           benchmarkMode_ = false;

  // ----- Constants -----
  static constexpr size_t max_iterations_ = 10000;

  // ----- Data Structures -----
  std::vector<HashDigest> all_digests_;
  std::vector<std::unordered_set<std::array<uint8_t, 3>>> iterationPatterns_;

  // ----- Helper: Generate a random digest for Benchmark Mode -----
  void generateRandomDigest(HashDigest& digest) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);

    for (auto& byte : digest) {
      byte = dist(gen);
    }
  }

  // ----- Helper: Extract All 3-byte Patterns from a Digest -----
  void extractPatterns(const HashDigest& digest,
                       std::unordered_set<std::array<uint8_t, 3>>& patternSet) {
    // i < j < k in [0..31]
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

  // ----- Helper: Check for "2-step cycle repeated twice" -----
  // i.e. p0 in D_i & D_i+2, and p1 in D_i+1 & D_i+3
  bool checkTwoStepCycle(size_t i) {
    if (i + 3 >= iterationPatterns_.size()) {
      return false;
    }

    const auto& pat_i   = iterationPatterns_[i];
    const auto& pat_i1  = iterationPatterns_[i + 1];
    const auto& pat_i2  = iterationPatterns_[i + 2];
    const auto& pat_i3  = iterationPatterns_[i + 3];

    // We want p0 in pat_i and pat_i2, p1 in pat_i1 and pat_i3
    for (const auto& p0 : pat_i) {
      if (pat_i2.find(p0) != pat_i2.end()) {
        for (const auto& p1 : pat_i1) {
          if (pat_i3.find(p1) != pat_i3.end()) {
            // Found a 2-step cycle
            visualizeCycle(i, p0, p1);
            return true;
          }
        }
      }
    }
    return false;
  }

  // ----- Helper: Show the 4 relevant digests and highlight the patterns -----
  void visualizeCycle(size_t i,
                      const std::array<uint8_t, 3>& p0,
                      const std::array<uint8_t, 3>& p1) const {
    std::cout << "\n--- 2-Step Cycle Repeated Twice Detected ---\n"
              << "We have:\n"
              << "  p0 in Digest[" << i     << "] and Digest[" << (i+2) << "]\n"
              << "  p1 in Digest[" << (i+1) << "] and Digest[" << (i+3) << "]\n\n";

    // Show the cycle visually
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

  // ----- Helper: Print a Sorted 3-Byte Pattern as Hex -----
  void printTripleHex(const std::array<uint8_t, 3>& triple) const {
    std::cout << "[ ";
    for (auto b : triple) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)b << " ";
    }
    std::cout << std::dec << "]";
  }

  // ----- Helper: Highlight the Bytes in the Digest that Match the Pattern -----
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
                  << std::hex << std::setw(2) << std::setfill('0')
                  << (int)b << "> ";
        patSet.erase(it);
      } else {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << (int)b << " ";
      }
    }
    std::cout << std::dec << "\n\n";
  }
};

#endif // HASH_CYCLE_MONITOR_H

