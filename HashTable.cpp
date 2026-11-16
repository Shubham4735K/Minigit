#include "HashTable.h"
#include <iostream>

using namespace std;

// Constructor: Initializes dynamic bucket pointer array with INITIAL_CAPACITY (10)
// Time Complexity: O(INITIAL_CAPACITY) = O(1)
HashTable::HashTable()
    : capacity(INITIAL_CAPACITY), elementCount(0) {
    buckets = new Node*[capacity];
    for (int i = 0; i < capacity; ++i) {
        buckets[i] = nullptr;
    }
}

// Destructor: Deallocates all linked list nodes and the dynamic bucket array
// Time Complexity: O(n + capacity) where n is total elements in the hash table
HashTable::~HashTable() {
    clear();
    delete[] buckets;
    buckets = nullptr;
    capacity = 0;
}

// Move Constructor: Transfers ownership of dynamic buckets and nodes in O(1) time
// Time Complexity: O(1)
HashTable::HashTable(HashTable&& other) noexcept
    : capacity(other.capacity), elementCount(other.elementCount), buckets(other.buckets) {
    other.capacity = 0;
    other.elementCount = 0;
    other.buckets = nullptr;
}

// Move Assignment Operator: Cleans up current table and moves resources in O(1) time
// Time Complexity: O(n_current + capacity_current) for cleanup, then O(1) pointer swap
HashTable& HashTable::operator=(HashTable&& other) noexcept {
    if (this != &other) {
        clear();
        delete[] buckets;

        capacity = other.capacity;
        elementCount = other.elementCount;
        buckets = other.buckets;

        other.capacity = 0;
        other.elementCount = 0;
        other.buckets = nullptr;
    }
    return *this;
}

// Clears all linked list nodes across all buckets and resets count to 0
// Time Complexity: O(n + capacity)
void HashTable::clear() {
    if (buckets == nullptr) return;
    for (int i = 0; i < capacity; ++i) {
        Node* current = buckets[i];
        while (current != nullptr) {
            Node* temp = current;
            current = current->next;
            delete temp; // Free dynamically allocated node
        }
        buckets[i] = nullptr;
    }
    elementCount = 0;
}

// Polynomial rolling hash mapped to bucket range [0, capacity - 1]
// Formula: index = (sum(c * 31^i)) % capacity
// Time Complexity: O(L) where L is string key length
int HashTable::hashFunction(const string& key) const {
    if (capacity <= 0) return 0;
    unsigned long long hash = 0;
    const unsigned long long primeMultiplier = 31;

    for (char c : key) {
        hash = hash * primeMultiplier + static_cast<unsigned char>(c);
    }

    return static_cast<int>(hash % capacity);
}

// Expose bucket index calculation for testing and viva demonstration
// Time Complexity: O(L) where L is key length
int HashTable::getBucketIndex(const string& key) const {
    return hashFunction(key);
}

// Dynamic Resizing & Rehashing:
// Doubles capacity when load factor > 0.75 (count / capacity > 0.75).
// Relinks existing Node pointers into new buckets WITHOUT reallocating or deleting Node objects.
// Time Complexity: O(n + capacity) where n is element count
// Space Complexity: O(new_capacity) for new bucket pointer array
void HashTable::rehash() {
    int oldCapacity = capacity;
    Node** oldBuckets = buckets;

    // Double capacity to reduce chain lengths and preserve O(1) average lookup
    capacity = oldCapacity * 2;
    buckets = new Node*[capacity];
    for (int i = 0; i < capacity; ++i) {
        buckets[i] = nullptr;
    }

    // Relink existing nodes into the new bucket array without any Node reallocations
    for (int i = 0; i < oldCapacity; ++i) {
        Node* current = oldBuckets[i];
        while (current != nullptr) {
            Node* nextNode = current->next; // Store next node pointer before unlinking

            int newIndex = hashFunction(current->key); // Recalculate bucket index with doubled capacity

            // Prepend existing node to head of new bucket chain (O(1))
            current->next = buckets[newIndex];
            buckets[newIndex] = current;

            current = nextNode;
        }
    }

    // Free the old pointer array (nodes themselves are preserved and relinked)
    delete[] oldBuckets;
}

// Insert operation:
// 1. If key exists in bucket chain: update value in-place (element count unchanged).
// 2. If key is new: create node and prepend to head of chain (O(1)), increment elementCount.
// 3. If load factor (elementCount / capacity) > 0.75: trigger rehash().
// Time Complexity: O(1) average, O(n) worst-case collision chain; amortized O(1) with resizing
void HashTable::insert(const string& key, const string& value) {
    if (capacity <= 0 || buckets == nullptr) {
        capacity = INITIAL_CAPACITY;
        buckets = new Node*[capacity];
        for (int i = 0; i < capacity; ++i) buckets[i] = nullptr;
    }

    int index = hashFunction(key);

    // 1. Search the bucket's linked list to check if key already exists
    Node* current = buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            // Key already present: update value in-place without increasing count
            current->value = value;
            return;
        }
        current = current->next;
    }

    // 2. Key does not exist: allocate new node and prepend at bucket head (O(1))
    Node* newNode = new Node(key, value);
    newNode->next = buckets[index];
    buckets[index] = newNode;
    elementCount++;

    // 3. Dynamic resizing check: load factor threshold = 0.75
    if (static_cast<double>(elementCount) / capacity > 0.75) {
        rehash();
    }
}

// Get operation:
// Traverses linked list at key's bucket index.
// Time Complexity: O(1) average, O(k) where k is bucket chain length (worst-case O(n))
string HashTable::get(const string& key) const {
    if (capacity <= 0 || buckets == nullptr) return "";
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
// Returns true if key exists in hash table, false otherwise.
// Time Complexity: O(1) average, O(k) worst-case
bool HashTable::contains(const string& key) const {
    if (capacity <= 0 || buckets == nullptr) return false;
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
// Searches for key in bucket list, unlinks node from singly linked list, and frees memory.
// Time Complexity: O(1) average, O(k) worst-case
bool HashTable::remove(const string& key) {
    if (capacity <= 0 || buckets == nullptr) return false;
    int index = hashFunction(key);

    Node* current = buckets[index];
    Node* prev = nullptr;

    while (current != nullptr) {
        if (current->key == key) {
            if (prev == nullptr) {
                // Node to remove is head of bucket list
                buckets[index] = current->next;
            } else {
                // Node to remove is in middle or end of list
                prev->next = current->next;
            }

            delete current; // Free dynamic node memory
            elementCount--;
            return true;
        }
        prev = current;
        current = current->next;
    }

    return false; // Key not found
}

// Retrieves all key-value pairs stored across all buckets
// Used for serialization to disk and snapshot comparisons.
// Time Complexity: O(n + capacity)
vector<pair<string, string>> HashTable::getAll() const {
    vector<pair<string, string>> items;
    if (buckets == nullptr) return items;

    for (int i = 0; i < capacity; ++i) {
        Node* current = buckets[i];
        while (current != nullptr) {
            items.push_back({current->key, current->value});
            current = current->next;
        }
    }
    return items;
}

// Visualizes buckets and collision chains (for viva and debugging)
// Time Complexity: O(n + capacity)
void HashTable::printTable() const {
    if (buckets == nullptr) return;

    for (int i = 0; i < capacity; ++i) {
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
