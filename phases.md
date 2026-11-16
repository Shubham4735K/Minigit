# MiniGit — Project Phases & Architecture Reference

This reference document tracks the development phases of **MiniGit**, detailing completed features, underlying Data Structures & Algorithms (DSA), file structures, and planned future phases.

---

## Roadmap Summary

| Phase | Title | Command / Feature | DSA Concept | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1** | Repository Initialization | `minigit init` | File/Directory I/O, Persistent Counter initialization | **Completed** |
| **Phase 2** | Deterministic Hashing | `minigit hash <file>` | Polynomial Rolling Hash ($O(n)$) | **Completed** |
| **Phase 3** | Custom Hash Table & Dynamic Resizing | `minigit test-hashtable` | Separate Chaining, Dynamic Resizing & Node Relinking | **Completed** |
| **Phase 4** | Staging Area & CAS | `minigit add <file>` | Hash Table serialization, Collision-safe Content-Addressable Storage | **Completed** |
| **Phase 5** | Commit Snapshots | `minigit commit "<msg>"` | Singly Linked List of Commits, Persistent ID Counter, Duplicate Suppression | **Completed** |
| **Phase 6** | Commit History Log | `minigit log` | Linked List Traversal ($O(k)$) | **Completed** |
| **Phase 7** | Rollback & Object Storage | `minigit checkout <id> [--force]` | State Restoration, Uncommitted Changes Guard, Tracked File Pruning | **Completed** |
| **Phase 8** | Branches & Pointers | `minigit branch <name>` | Tree/Graph branching, Named pointers | *Next up* |
| **Phase 9** | Line-by-Line Diff | `minigit diff` | Longest Common Subsequence (LCS) / DP | *Planned* |
| **Phase 10** | Three-Way Merge | `minigit merge <branch>` | Lowest Common Ancestor (LCA) on DAG | *Planned* |

---

## Detailed Phase Breakdown

### Phase 1: Repository Initialization (`minigit init`)
* **Status:** Completed
* **Purpose:** Sets up the root environment for tracking files without disturbing the user's workspace.
* **Key Actions:**
  * Checks for pre-existing `.minigit/` to prevent data loss.
  * Creates hidden directory `.minigit/`.
  * Creates `.minigit/objects/` for binary blob storage.
  * Creates `.minigit/HEAD` pointing initially to branch `main`.
  * Creates `.minigit/counter` initialized to `0` for globally unique commit IDs.
* **Files Involved:** `main.cpp` (`initRepository()`).

---

### Phase 2: Content Fingerprinting (`minigit hash <file>`)
* **Status:** Completed
* **Purpose:** Generates a deterministic, fixed-size numerical digest of any file's content.
* **DSA Concept:** **Polynomial Rolling Hash Function**
  $$\text{hash} = \sum_{i=0}^{n-1} c_i \cdot 31^{n-1-i} \pmod{2^{64}}$$
* **Implementation Details:**
  * Multiplier `31` (prime) prevents anagram collisions (e.g., `"cat"` vs `"act"`).
  * `unsigned long long` provides cross-platform deterministic overflow modulo ($2^{64}$).
* **Files Involved:** `Hash.h`, `Hash.cpp`, `main.cpp` (`hashFile()`).

---

### Phase 3: In-Memory Key-Value Store (`HashTable`) & Dynamic Resizing
* **Status:** Completed (Enhanced with Dynamic Resizing)
* **Purpose:** Custom dictionary (`filename → hash`) without using `<unordered_map>`.
* **DSA Concept:** **Hash Table with Separate Chaining & Dynamic Resizing**
  * Initial bucket array size: `INITIAL_CAPACITY = 10` (retained so small-dataset collision demonstrations remain intact).
  * Dynamic bucket storage: `Node** buckets`.
  * Load Factor Threshold: $\alpha = \frac{\text{elementCount}}{\text{capacity}} > 0.75$.
  * **Dynamic Rehashing by Relinking:** When load factor exceeds 0.75, doubles capacity ($10 \to 20 \to 40 \dots$) and rehashes by relinking existing `Node` pointers without deleting or re-allocating `Node` objects ($O(n)$ time, $0$ Node allocations).
  * `insert(key, value)`: Updates in-place if key exists; prepends to head ($O(1)$) if new.
  * `get(key)` / `contains(key)`: Traverses bucket chain ($O(1)$ average, $O(n)$ worst-case).
  * `remove(key)`: Unlinks node from singly linked list and frees dynamic memory (`delete`).
  * `getAll()`: Linear traversal of all buckets for disk persistence and snapshot comparisons.
* **Verification:** `minigit test-hashtable` exercises collision chaining (Bucket 0) and dynamic resizing across 30 elements ($10 \to 40$).
* **Files Involved:** `HashTable.h`, `HashTable.cpp`.

---

### Phase 4: Staging Area & Collision-Safe CAS (`minigit add <file>`)
* **Status:** Completed (Enhanced with Object Collision Safety)
* **Purpose:** Prepares specific files for inclusion in the next commit snapshot.
* **Key Actions:**
  * Computes file hash using `calculateHash()`.
  * **Collision-Safe Object Store:** Checks if `.minigit/objects/<hash>` already exists. If present, verifies bitwise equality with incoming content. If contents differ (hash collision), reports an error and aborts instead of corrupting the blob.
  * Loads existing `.minigit/staging` into in-memory `HashTable`.
  * Inserts or updates the entry (`filename → hash`).
  * Serializes updated table back to `.minigit/staging` as pipe-delimited records (`filename|hash`).
* **Files Involved:** `main.cpp` (`addFile()`).

---

### Phase 5: Commit Snapshots (`minigit commit "<msg>"`)
* **Status:** Completed (Enhanced with Persistent Counter & Idempotency)
* **Purpose:** Creates a permanent, immutable record of staged files.
* **DSA Concept:** **Singly Linked List of Commits & Persistent Global Counter**
  * `HEAD` acts as the pointer to the latest commit.
  * **Persistent Commit Counter (`.minigit/counter`):**
    * New commit ID is generated from `counter + 1` (e.g., `C001`, `C002`, `C003`).
    * Solves checkout collision bug: Checking out an older commit (`checkout C001`) and making a new commit will produce `C003` with `parent=C001`, leaving `C002` intact and avoiding overwrites.
    * Rebuilds counter automatically by scanning `.minigit/commits/` if the file is ever missing.
  * **Duplicate Commit Suppression:** Compares staging `HashTable` with parent commit snapshot using `loadSnapshot(commitId)`. If identical, prints `"Nothing to commit, working tree clean"` and aborts.
  * **Message Validation:** Rejects messages containing newline characters and trims/rejects empty or whitespace-only messages.
* **Files Involved:** `main.cpp` (`createCommit()`, `getNextCommitNumber()`, `loadSnapshot()`).

---

### Phase 6: Commit History Log (`minigit log`)
* **Status:** Completed
* **Purpose:** Traverses the commit graph from `HEAD` backward to the initial commit, printing formatted history.
* **DSA Concept:** **Linked List Traversal**
  * Start at `current = read(HEAD)`.
  * While `current != "NULL"`:
    * Print ID, message, parent, and tracked files.
    * `current = commit.parent`.
* **Complexity:** $O(k)$ where $k$ is the number of commits.
* **Files Involved:** `main.cpp` (`printCommitLog()`).

---

### Phase 7: Rollback / Checkout (`minigit checkout <commitId> [--force]`)
* **Status:** Completed (Enhanced with Safety Guard & File Pruning)
* **Purpose:** Restores workspace files to match any historical snapshot.
* **DSA Concept:** **Content-Addressable Storage (CAS) & State Restoration**
  * **Uncommitted Changes Guard:** Checks if staging or working directory files differ from the current `HEAD` commit snapshot. Refuses to overwrite files unless `--force` is passed.
  * **Tracked File Pruning:** Any files tracked in the previous commit that do not exist in the target snapshot are automatically removed from the working directory.
  * Restores all target files from `.minigit/objects/<hash>`.
  * Synchronizes `.minigit/staging` to match the target snapshot.
  * Moves `HEAD` pointer to `<commitId>`.
* **Files Involved:** `main.cpp` (`checkoutCommit()`).

---

### Phase 8: Branches (`minigit branch <name>`)
* **Status:** Planned
* **Purpose:** Supports branching workflows by tracking multiple heads.
* **DSA Concept:** **Graph / Tree Branching & Named Pointers**

---

### Phase 9: File Difference (`minigit diff`)
* **Status:** Planned
* **Purpose:** Compares the working file against the staged or last committed version.
* **DSA Concept:** **Longest Common Subsequence (LCS) / Dynamic Programming**

---

### Phase 10: Three-Way Merge (`minigit merge <branch>`)
* **Status:** Planned
* **Purpose:** Automatically merges changes from another branch into the current branch.
* **DSA Concept:** **Lowest Common Ancestor (LCA)**
