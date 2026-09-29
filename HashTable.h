#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <string>
#include <vector>
#include <utility>

// Node structure representing an element in a bucket's linked list (separate chaining)
struct Node {
    std::string key;    // e.g., filename ("main.cpp")
    std::string value;  // e.g., file hash ("123456789")
    Node* next;         // pointer to the next node in the chain

    // Constructor for easy node creation
    Node(const std::string& k, const std::string& v)
        : key(k), value(v), next(nullptr) {}
};

// Custom Hash Table using fixed-size bucket array and separate chaining
class HashTable {
private:
    static const int TABLE_SIZE = 10; // Fixed small size for educational demonstration
    Node* buckets[TABLE_SIZE];        // Array of bucket head pointers

    // Internal hash function to map a string key to a bucket index
    int hashFunction(const std::string& key) const;

public:
    HashTable();
    ~HashTable();

    // Disable copy constructor and copy assignment to avoid accidental shallow copying
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Core operations
    void insert(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;
    bool contains(const std::string& key) const;
    bool remove(const std::string& key);

    // Retrieve all key-value pairs (used to persist staging area)
    std::vector<std::pair<std::string, std::string>> getAll() const;

    // Helpers for viva demonstration and collision testing
    int getBucketIndex(const std::string& key) const;
    void printTable() const;
    void clear();
};

#endif // HASHTABLE_H
