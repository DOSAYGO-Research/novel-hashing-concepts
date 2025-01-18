#include <iostream>
#include "hash-cycle-monitor.h"
#include <random>

HashDigest generateRandomDigest() {
  HashDigest digest;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint8_t> dist(0, 255);

  for (auto& byte : digest) {
    byte = dist(gen);
  }
  return digest;
}

int main() {
  // Example parameters
  const size_t reruns = 5;
  const seed_t seed = 12345;
  const uint32_t pattern_width = 3;
  const size_t max_length = 2;
  const size_t min_revolutions = 2;

  // Create once
  HashCycleMonitor monitor(pattern_width, max_length, min_revolutions);

  for (size_t run = 1; run <= reruns; ++run) {
    // Reset for a fresh run
    monitor.reset();

    std::cout << "\n--- Run " << run << " ---\n";
    HashDigest initial_digest = generateRandomDigest();

    // Show the initial random digest
    std::cout << "Initial Digest: ";
    for (auto byte : initial_digest) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << "\n";

    // Analyze
    monitor.analyze(initial_digest.data(), initial_digest.size(), seed);
  }

  return 0;
}

