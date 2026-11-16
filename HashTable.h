#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <string>
#include <vector>
#include <utility>

using namespace std;

// Node structure representing an element in a bucket's linked list (separate chaining)
// Data Structure: Singly Linked List Node
// Time Complexity: O(1) creation, memory allocation for single node
struct Node {
    string key;    // e.g., filename ("main.cpp")
    string value;  // e.g., file hash ("123456789")
    Node* next;    // pointer to next node in collision chain

    // Constructor for easy node creation
    Node(const string& k, const string& v)
        : key(k), value(v), next(nullptr) {}
};

// Custom Hash Table using dynamic separate chaining
// DSA Concept: Hash Table with Separate Chaining & Dynamic Resizing
// - Initial Capacity: 10 buckets (preserves collision demonstration for small datasets)
// - Load Factor Threshold: 0.75 (count / capacity > 0.75 triggers doubling of capacity)
// - Rehashing: Doubles bucket array; relinks existing Node pointers directly (O(n) time, 0 new Node allocations)
// Time Complexities:
//   - insert: O(1) average, O(n) worst-case; amortized O(1) including rehashing
//   - get: O(1) average, O(n) worst-case (chain length)
//   - contains: O(1) average, O(n) worst-case
//   - remove: O(1) average, O(n) worst-case
//   - rehash: O(n) time to distribute n nodes into new buckets; O(new_capacity) memory for bucket pointer array
class HashTable {
private:
    static const int INITIAL_CAPACITY = 10; // Initial bucket count (keeps collision demo intact)
    int capacity;                           // Dynamic table size (number of buckets)
    int elementCount;                       // Number of active key-value entries
    Node** buckets;                         // Dynamic array of bucket head pointers (Node* buckets[capacity])

    // Internal hash function: maps string key to bucket index [0, capacity - 1]
    int hashFunction(const string& key) const;

    // Resizes bucket array to 2 * capacity and relinks existing nodes (no Node reallocations)
    void rehash();

public:
    HashTable();
    ~HashTable();

    // Disable copy constructor and copy assignment to prevent accidental shallow copying of dynamic pointers
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Move constructor & move assignment (efficient O(1) transfer for helper return values)
    HashTable(HashTable&& other) noexcept;
    HashTable& operator=(HashTable&& other) noexcept;

    // Core operations
    void insert(const string& key, const string& value);
    string get(const string& key) const;
    bool contains(const string& key) const;
    bool remove(const string& key);

    // Retrieve all key-value pairs (used to persist staging area and compare snapshots)
    vector<pair<string, string>> getAll() const;

    // Inspection and testing helpers
    int getBucketIndex(const string& key) const;
    void printTable() const;
    void clear();

    // Size queries for load factor tracking and viva demonstration
    int size() const { return elementCount; }
    int getCapacity() const { return capacity; }
    int getTableSize() const { return capacity; }
    bool empty() const { return elementCount == 0; }
};

#endif // HASHTABLE_H
