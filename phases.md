# MiniGit — Project Phases & Architecture Reference

This reference document tracks the development phases of **MiniGit**, detailing completed features, underlying Data Structures & Algorithms (DSA), file structures, and planned future phases.

---

## Roadmap Summary

| Phase | Title | Command / Feature | DSA Concept | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1** | Repository Initialization | `minigit init` | File/Directory I/O, State initialization | **Completed** |
| **Phase 2** | Deterministic Hashing | `minigit hash <file>` | Polynomial Rolling Hash ($O(n)$) | **Completed** |
| **Phase 3** | Custom Hash Table | `minigit test-hashtable` | Hash Table, Separate Chaining (Linked Lists) | **Completed** |
| **Phase 4** | Staging Area | `minigit add <file>` | Hash Table serialization, String parsing | **Completed** |
| **Phase 5** | Commit Snapshots | `minigit commit "<msg>"` | Singly Linked List of Commits (Parent pointers) | **Completed** |
| **Phase 6** | Commit History Log | `minigit log` | Linked List Traversal ($O(k)$) | **Completed** |
| **Phase 7** | Rollback & Object Storage | `minigit checkout <id>` | Content-Addressable Storage (CAS / Blobs) | **Completed** |
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

### Phase 3: In-Memory Key-Value Store (`HashTable`)
* **Status:** Completed
* **Purpose:** Custom dictionary (`filename → hash`) without using `<unordered_map>`.
* **DSA Concept:** **Hash Table with Separate Chaining**
  * Fixed bucket array size: `TABLE_SIZE = 10`.
  * Buckets store singly linked list nodes (`Node* buckets[10]`).
  * `insert(key, value)`: Updates in-place if key exists; prepends to head ($O(1)$) if new.
  * `get(key)` / `contains(key)`: Traverses bucket chain ($O(1)$ average, $O(n)$ worst-case).
  * `remove(key)`: Unlinks node and frees dynamic memory (`delete`) to prevent leaks.
  * `getAll()`: Linear traversal of all buckets for disk persistence.
* **Verification:** `minigit test-hashtable` exercises collisions (e.g. `main.cpp` and `beta.txt` landing in Bucket 0).
* **Files Involved:** `HashTable.h`, `HashTable.cpp`.

---

### Phase 4: Staging Area (`minigit add <file>`)
* **Status:** Completed
* **Purpose:** Prepares specific files for inclusion in the next commit snapshot.
* **Key Actions:**
  * Computes file hash using `calculateHash()`.
  * Stores a copy of file contents into Content-Addressable Storage (`.minigit/objects/<hash>`).
  * Deserializes existing `.minigit/staging` into in-memory `HashTable`.
  * Inserts or updates the entry (`filename → hash`).
  * Serializes updated table back to `.minigit/staging` as pipe-delimited records (`filename|hash`).
* **Files Involved:** `main.cpp` (`addFile()`).

---

### Phase 5: Commit Snapshots (`minigit commit "<msg>"`)
* **Status:** Completed
* **Purpose:** Creates a permanent, immutable record of staged files.
* **DSA Concept:** **Singly Linked List of Commits**
  * `HEAD` acts as the head pointer.
  * First commit points to `parent=NULL`.
  * Subsequent commits point to previous commit ID (e.g., `C002 -> parent=C001`).
  * Commit snapshot stored at `.minigit/commits/<commitId>`:
    ```text
    id=C001
    message=Initial commit
    parent=NULL

    main.cpp|5019670368770650375
    ```
* **Files Involved:** `main.cpp` (`createCommit()`).

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

### Phase 7: Rollback / Checkout (`minigit checkout <commitId>`)
* **Status:** Completed
* **Purpose:** Restores workspace files to match any historical snapshot.
* **DSA Concept:** **Content-Addressable Storage (CAS) & State Restoration**
  * Reads `.minigit/commits/<commitId>`.
  * For each `filename|hash`:
    * Looks up content directly from `.minigit/objects/<hash>`.
    * Restores file in the working directory.
  * Synchronizes `.minigit/staging` to match the snapshot.
  * Moves `HEAD` pointer to `<commitId>`.
* **Files Involved:** `main.cpp` (`checkoutCommit()`).

---

### Phase 8: Branches (`minigit branch <name>`)
* **Status:** Planned
* **Purpose:** Supports branching workflows by tracking multiple heads.
* **DSA Concept:** **Graph / Tree Branching & Named Pointers**
  * `.minigit/refs/heads/<branchName>` storing commit IDs.
  * `.minigit/HEAD` pointing to active branch reference (e.g. `ref: refs/heads/main`).

---

### Phase 9: File Difference (`minigit diff`)
* **Status:** Planned
* **Purpose:** Compares the working file against the staged or last committed version.
* **DSA Concept:** **Longest Common Subsequence (LCS) / Dynamic Programming**
  * $O(m \times n)$ 2D matrix dynamic programming to compute minimal edit script (insertions/deletions).

---

### Phase 10: Three-Way Merge (`minigit merge <branch>`)
* **Status:** Planned
* **Purpose:** Automatically merges changes from another branch into the current branch.
* **DSA Concept:** **Lowest Common Ancestor (LCA)**
  * Traverses backward through commit parents to find common ancestor node in the DAG.
  * Applies three-way reconciliation algorithm (ancestor vs base vs incoming).

