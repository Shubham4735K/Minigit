#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <string>
#include <vector>
#include <utility>

using namespace std;

// Node structure representing an element in a bucket's linked list (separate chaining)
struct Node {
    string key;    // e.g., filename ("main.cpp")
    string value;  // e.g., file hash ("123456789")
    Node* next;         // pointer to the next node in the chain

    // Constructor for easy node creation
    Node(const string& k, const string& v)
        : key(k), value(v), next(nullptr) {}
};

// Custom Hash Table using fixed-size bucket array and separate chaining
class HashTable {
private:
    static const int TABLE_SIZE = 10; // Fixed small size for demonstration
    Node* buckets[TABLE_SIZE];        // Array of bucket head pointers

    // Internal hash function to map a string key to a bucket index
    int hashFunction(const string& key) const;

public:
    HashTable();
    ~HashTable();

    // Disable copy constructor and copy assignment to avoid accidental shallow copying
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Core operations
    void insert(const string& key, const string& value);
    string get(const string& key) const;
    bool contains(const string& key) const;
    bool remove(const string& key);

    // Retrieve all key-value pairs (used to persist staging area)
    vector<pair<string, string>> getAll() const;

    // Helpers for viva demonstration and collision testing
    int getBucketIndex(const string& key) const;
    void printTable() const;
    void clear();
};

#endif // HASHTABLE_H
