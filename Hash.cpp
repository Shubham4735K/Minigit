#include "Hash.h"

using namespace std;

// Educational polynomial hash function:
//
// File content -> Hash function -> Hash value (fingerprint)
//
// Algorithm:
//   hash = 0
//   for each character c in content:
//       hash = hash * 31 + c
//
// Explanation:
// 1. Prime Multiplier (31):
//    Multiplying by a prime number (31) distributes character values evenly
//    across the number range, preventing simple anagram collisions (e.g., "cat" vs "act").
// 2. Unsigned Integer Overflow:
//    Using 'unsigned long long' provides well-defined modulo arithmetic on overflow,
//    ensuring the computation is consistent and deterministic on all systems.
string calculateHash(const string& content) {
    unsigned long long hash = 0;
    const unsigned long long primeMultiplier = 31;

    for (char c : content) {
        // Cast to unsigned char to safely handle any byte/character values
        hash = hash * primeMultiplier + static_cast<unsigned char>(c);
    }

    // Convert the numeric hash value to a decimal string
    return to_string(hash);
}
