#include "HashTable.h"
#include <iostream>

using namespace std;

// Constructor: Initialize all bucket pointers to nullptr
HashTable::HashTable() {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        buckets[i] = nullptr;
    }
}

// Destructor: Free all dynamically allocated nodes across all buckets
HashTable::~HashTable() {
    clear();
}

// Clears all entries from the hash table and frees their memory
void HashTable::clear() {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        Node* current = buckets[i];
        while (current != nullptr) {
            Node* temp = current;
            current = current->next;
            delete temp; // Free dynamically allocated node
        }
        buckets[i] = nullptr;
    }
}

// Internal hash function: Converts a string key into a valid bucket index
// Formula: index = (sum(c * 31^i)) % TABLE_SIZE
int HashTable::hashFunction(const string& key) const {
    unsigned long long hash = 0;
    const unsigned long long primeMultiplier = 31;

    for (char c : key) {
        hash = hash * primeMultiplier + static_cast<unsigned char>(c);
    }

    // Modulo operation maps the hash value into [0, TABLE_SIZE - 1]
    return static_cast<int>(hash % TABLE_SIZE);
}

// Expose bucket index calculation for testing and viva demonstration
int HashTable::getBucketIndex(const string& key) const {
    return hashFunction(key);
}

// Insert operation:
// If key already exists in the chain, update its value.
// Otherwise, create a new Node and insert it at the head of the chain.
void HashTable::insert(const string& key, const string& value) {
    int index = hashFunction(key);

    // 1. Search the bucket's linked list to check if key already exists
    Node* current = buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            // Key already present: update value and avoid duplicate node
            current->value = value;
            return;
        }
        current = current->next;
    }

    // 2. Key does not exist: create a new node and insert at head of list (O(1))
    Node* newNode = new Node(key, value);
    newNode->next = buckets[index];
    buckets[index] = newNode;
}

// Get operation:
// Traverses the linked list at the key's bucket index.
// Returns the value string if found, or an empty string "" if not found.
string HashTable::get(const string& key) const {
    int index = hashFunction(key);

    Node* current = buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            return current->value;
        }
        current = current->next;
    }

    return "";
}

// Contains operation:
// Returns true if the key exists in the hash table, false otherwise.
bool HashTable::contains(const string& key) const {
    int index = hashFunction(key);

    Node* current = buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            return true;
        }
        current = current->next;
    }

    return false;
}

// Remove operation:
// Searches for the key in its bucket list, updates linked list pointers,
// and deletes the node to prevent memory leaks.
bool HashTable::remove(const string& key) {
    int index = hashFunction(key);

    Node* current = buckets[index];
    Node* prev = nullptr;

    while (current != nullptr) {
        if (current->key == key) {
            if (prev == nullptr) {
                // Node to remove is the head of the bucket
                buckets[index] = current->next;
            } else {
                // Node to remove is in the middle or end
                prev->next = current->next;
            }

            delete current; // Free memory
            return true;
        }
        prev = current;
        current = current->next;
    }

    return false; // Key was not found
}

// Retrieves all key-value pairs stored across all buckets
// Traverses each bucket and its associated linked-list chain
vector<pair<string, string>> HashTable::getAll() const {
    vector<pair<string, string>> items;
    for (int i = 0; i < TABLE_SIZE; ++i) {
        Node* current = buckets[i];
        while (current != nullptr) {
            items.push_back({current->key, current->value});
            current = current->next;
        }
    }
    return items;
}

// Visualizes the buckets and chains (useful for college viva demonstrations)
void HashTable::printTable() const {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        cout << "Bucket [" << i << "]: ";
        Node* current = buckets[i];
        if (current == nullptr) {
            cout << "empty";
        } else {
            while (current != nullptr) {
                cout << "[" << current->key << " : " << current->value << "]";
                if (current->next != nullptr) {
                    cout << " -> ";
                }
                current = current->next;
            }
        }
        cout << endl;
    }
}
