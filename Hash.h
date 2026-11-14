#ifndef HASH_H
#define HASH_H

#include <string>

using namespace std;

// Computes a deterministic polynomial hash for the given string content
string calculateHash(const string& content);

#endif // HASH_H
