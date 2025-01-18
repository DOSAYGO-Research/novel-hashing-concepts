#include "hash-cycle-monitor.h"
#include <iostream>
#include <string>
#include <random>

// Utility function to generate a random initial digest
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
    const size_t reruns = 5;           // Number of test reruns
    const seed_t seed = 12345;         // Seed for the hash function
    const uint32_t pattern_width = 3;  // Width of the patterns to track
    const size_t max_length = 2;       // Max cycle length
    const size_t min_revolutions = 2;  // Minimum cycle recurrences

    // Initialize the monitor
    HashCycleMonitor monitor(pattern_width, max_length, min_revolutions);

    for (size_t run = 1; run <= reruns; ++run) {
        std::cout << "\n--- Run " << run << " ---\n";

        // Generate a random initial digest
        HashDigest initial_digest = generateRandomDigest();

        // Display the starting digest
        std::cout << "Initial Digest: ";
        for (const auto& byte : initial_digest) {
            std::cout << std::hex << (int)byte << " ";
        }
        std::cout << "\n";

        // Analyze the chain starting from the random digest
        monitor.analyze(initial_digest.data(), initial_digest.size(), seed);
    }

    return 0;
}

