#include <iostream>
#include <random>
#include <string>
#include "hash-cycle-monitor.h"

// Utility to generate a random 32-byte digest for the "normal" mode's initial input
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

  // Create the monitor once
  HashCycleMonitor monitor(pattern_width, max_length, min_revolutions);
  monitor.enableVerbose(true);

  for (size_t run = 1; run <= reruns; ++run) {
    //===============================
    // Normal "hash chain" run
    //===============================
    monitor.reset();            // Clear out old state
    monitor.enableBenchmarkMode(false);

    std::cout << "\n=== Normal Run " << run << " ===\n";

    // Generate random data for the initial input digest
    HashDigest initial_digest = generateRandomDigest();

    // Display the starting digest
    std::cout << "Initial Digest (hash mode): ";
    for (auto byte : initial_digest) {
      std::cout << std::hex << (int)byte << " ";
    }
    std::cout << std::dec << "\n";

    // Analyze with a real hash chain
    monitor.analyze(initial_digest.data(), initial_digest.size(), seed);

    //===============================
    // Benchmark "random vector" run
    //===============================
    monitor.reset();            // Fresh state
    monitor.enableBenchmarkMode(true);

    std::cout << "\n=== Benchmark Run " << run << " ===\n";
    std::cout << "No fixed initial digest; each iteration is random.\n";

    // In benchmark mode, the 'analyze' call ignores 'input' and 'len' anyway, so we can pass null
    monitor.analyze(nullptr, 0, seed);
  }

  return 0;
}

