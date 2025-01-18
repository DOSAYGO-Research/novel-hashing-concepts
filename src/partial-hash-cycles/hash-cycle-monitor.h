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
        result ^= hasher(val) + 0x9e3779b9 + (result << 6) + (result >> 2);
      }
      return result;
    }
  };
}

// ---------- Custom Hash for std::vector<uint8_t> ----------
struct VectorHash {
  size_t operator()(const std::vector<uint8_t>& vec) const {
    size_t hash = vec.size();
    for (auto v : vec) {
      hash ^= static_cast<size_t>(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
  }
};

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

class HashCycleMonitor {
public:
  // Constructor now takes an additional argument: cycle_order (L)
  HashCycleMonitor(uint32_t pattern_width, // W
                   size_t max_length,
                   size_t cycle_order,     // L
                   size_t min_revolutions) // R
    : pattern_width_(pattern_width)
    , max_length_(max_length)
    , cycle_order_(cycle_order)
    , min_revolutions_(min_revolutions) {}

  // Toggle random benchmark mode
  void enableBenchmarkMode(bool mode) {
    benchmarkMode_ = mode;
  }

  // Clear stored data so object can be reused
  void reset() {
    all_digests_.clear();
    iterationPatterns_.clear();
  }

  // Verbose on/off
  void enableVerbose(bool verbose) {
    verbose_ = verbose;
  }

  // Main driver for hashing or random generation
  void analyze(const void* input, size_t len, seed_t seed) {
    for (size_t i = 0; i < max_iterations_; ++i) {
      HashDigest digest;
      if (benchmarkMode_) {
        generateRandomDigest(digest);
      } else {
        hash<32, false>(input, len, seed, digest.data());
      }

      all_digests_.push_back(digest);

      // Extract W-byte patterns from digest.
      // Note: using std::vector<uint8_t> with our custom hash.
      std::unordered_set<std::vector<uint8_t>, VectorHash> patternSet;
      extractPatterns(digest, patternSet);
      iterationPatterns_.push_back(std::move(patternSet));

      // Check for cycle if we have enough iterations.
      if (i >= (cycle_order_ * min_revolutions_)) {
        if (checkCycle(i - (cycle_order_ * min_revolutions_) + 1)) {
          // Cycle found; break out if that's what you want.
          break;
        }
      }

      // For next iteration, feed digest back in if hash mode.
      if (!benchmarkMode_) {
        input = digest.data();
        len   = digest.size();
      }
    }
  }

private:
  // ----- Configuration -----
  const uint32_t pattern_width_; // W
  const size_t   max_length_;    // (Unused in this example, but can be used to limit iterations)
  const size_t   cycle_order_;   // L
  const size_t   min_revolutions_; // R
  bool           verbose_       = false;
  bool           benchmarkMode_ = false;

  // ----- Constants -----
  static constexpr size_t max_iterations_ = 10000;

  // ----- Data Storage -----
  std::vector<HashDigest> all_digests_;
  // Note: Use our custom hash for each vector pattern.
  std::vector<std::unordered_set<std::vector<uint8_t>, VectorHash>> iterationPatterns_;

  // Helper: Generate a random digest for benchmark mode
  void generateRandomDigest(HashDigest& digest) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);

    for (auto& byte : digest) {
      byte = dist(gen);
    }
  }

  // Helper: Extract all W-byte patterns (combinations) from a digest.
  // Each pattern is stored as a sorted vector<uint8_t>.
  void extractPatterns(const HashDigest& digest,
                       std::unordered_set<std::vector<uint8_t>, VectorHash>& patternSet) {
    // Create a vector holding indices for the combination
    std::vector<size_t> indices(pattern_width_);
    for (size_t i = 0; i < pattern_width_; ++i) {
      indices[i] = i;
    }

    while (true) {
      std::vector<uint8_t> pattern(pattern_width_);
      for (size_t x = 0; x < pattern_width_; ++x) {
        pattern[x] = digest[indices[x]];
      }
      // Normalize order
      std::sort(pattern.begin(), pattern.end());
      patternSet.insert(pattern);

      if (!nextCombination(indices, digest.size(), pattern_width_)) {
        break;
      }
    }
  }

  // Generate the next combination (indices) in lexicographic order.
  bool nextCombination(std::vector<size_t>& indices, size_t n, size_t k) {
    for (int i = k - 1; i >= 0; --i) {
      if (indices[i] < n - (k - i)) {
        ++indices[i];
        for (size_t j = i + 1; j < k; ++j) {
          indices[j] = indices[j - 1] + 1;
        }
        return true;
      }
    }
    return false;
  }

  // Check if there is a cycle of order L with at least R revolutions,
  // starting at iteration "startIndex".
  bool checkCycle(size_t startIndex) {
    size_t neededEnd = startIndex + (min_revolutions_ * cycle_order_) - 1;
    if (neededEnd >= iterationPatterns_.size()) {
      return false; // Not enough iterations yet.
    }

    // For each offset in [0, L-1], find a candidate pattern that repeats R times.
    std::vector<std::vector<uint8_t>> foundCyclePatterns(cycle_order_);
    for (size_t offset = 0; offset < cycle_order_; ++offset) {
      bool candidateFound = false;
      const auto& baseSet = iterationPatterns_[startIndex + offset];
      for (const auto& candidate : baseSet) {
        bool works = true;
        for (size_t r = 1; r < min_revolutions_; ++r) {
          size_t idx = startIndex + offset + r * cycle_order_;
          if (iterationPatterns_[idx].find(candidate) == iterationPatterns_[idx].end()) {
            works = false;
            break;
          }
        }
        if (works) {
          foundCyclePatterns[offset] = candidate;
          candidateFound = true;
          break;
        }
      }
      if (!candidateFound) {
        return false;
      }
    }

    visualizeCycle(startIndex, foundCyclePatterns);
    return true;
  }

  // Visualize the found cycle.
  void visualizeCycle(size_t startIndex,
                      const std::vector<std::vector<uint8_t>>& cyclePatterns) const {
    std::cout << "\n--- Cycle of Order " << cycle_order_ << " Detected, Repeated "
              << min_revolutions_ << " Times ---\n";
    std::cout << "Patterns:\n";
    for (size_t i = 0; i < cyclePatterns.size(); ++i) {
      std::cout << "  offset[" << i << "]: ";
      printPatternHex(cyclePatterns[i]);
      std::cout << "\n";
    }
    size_t endIndex = startIndex + min_revolutions_ * cycle_order_;
    std::cout << "\nCycle spans iteration " << startIndex << " to " << (endIndex - 1) << "\n\n";
  }

  // Print a pattern of size W in hexadecimal.
  void printPatternHex(const std::vector<uint8_t>& pattern) const {
    std::cout << "[";
    for (auto b : pattern) {
      std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    std::cout << " ]" << std::dec;
  }
};

#endif // HASH_CYCLE_MONITOR_H

