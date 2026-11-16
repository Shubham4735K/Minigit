# MiniGit — Simple Version Control System using Data Structures

A lightweight command-line version-control project designed for 2nd-year Data Structures and Algorithms (DSA). MiniGit demonstrates core data structure concepts step-by-step.

## Current Project Status
**Phase 7 (Enhanced) — Dynamic Hash Table, Safe Checkout, Object Store & Linked Commits**

---

## Supported Commands

### 1. Initialize Repository
```bash
minigit init
```
- Checks if a `.minigit` directory already exists to prevent overwriting existing data.
- Creates a hidden `.minigit/` directory and `.minigit/objects/` store for content blobs.
- Initializes the `HEAD` file pointing to the default branch (`main`).
- Initializes a persistent commit counter file (`.minigit/counter`) to `0`.

### 2. Compute File Hash
```bash
minigit hash <filename>
```
- Checks that the specified file exists and is readable.
- Computes a deterministic polynomial hash of the file's contents.
- Prints the filename and resulting hash value.

### 3. Stage a File
```bash
minigit add <filename>
```
- Checks that the current directory is an initialized MiniGit repository.
- Computes the file's hash using `calculateHash()`.
- **Collision-Safe Object Storage:** Checks if `.minigit/objects/<hash>` already exists. Verifies byte-level equality to detect real hash collisions and abort if content differs.
- Saves the file's content into Content-Addressable Storage (`.minigit/objects/<hash>`).
- Loads existing staged files from `.minigit/staging` into the custom in-memory `HashTable`.
- Inserts or updates the file's entry (`filename → hash`) in the `HashTable`.
- Persists all staged entries back to `.minigit/staging`.

Example:
```bash
minigit add main.cpp
```
Output:
```text
Staged: main.cpp
Hash: 5019670368770650375
```

### 4. Commit Staged Snapshot
```bash
minigit commit "<message>"
```
- Verifies `.minigit` repository exists.
- Validates commit message: rejects newline characters and rejects empty or whitespace-only messages.
- Checks that `.minigit/staging` exists and contains staged files.
- Reads staged entries into the custom `HashTable`.
- **Duplicate Commit Guard:** Compares the staging table with the parent commit's snapshot using `loadSnapshot(commitId)`. If identical, prints `"Nothing to commit, working tree clean"` and aborts.
- **Persistent Global Counter:** Reads `.minigit/counter` to assign a globally unique commit ID (`C001`, `C002`, `C003`, ...), eliminating commit overwrites after checkout.
- Creates a permanent snapshot file at `.minigit/commits/<commitId>`.
- Updates `.minigit/HEAD` to point to the newly created commit.

Example:
```bash
minigit commit "Initial commit"
```
Output:
```text
[C001] Initial commit
```

### 5. Checkout / Restore Commit Snapshot
```bash
minigit checkout <commitId> [--force]
```
- Verifies `.minigit` repository and target commit exist.
- **Safety Check:** Checks if staging or working files differ from the current `HEAD` commit snapshot (uncommitted changes). Refuses with an error unless `--force` is passed.
- **File Pruning:** Deletes files that were tracked in the previous commit but do not exist in the target snapshot.
- Restores file contents from `.minigit/objects/<hash>` into the working directory.
- Synchronizes `.minigit/staging` with the checked-out snapshot.
- Moves `.minigit/HEAD` pointer to `<commitId>`.

Example:
```bash
minigit checkout C001
```
Output:
```text
Switched to commit C001 ("Initial commit")
Restored 1 file(s).
```

### 6. View Commit History Log
```bash
minigit log
```
- Reads the current commit pointer from `.minigit/HEAD`.
- Traverses backward along parent pointers (`HEAD -> parent -> parent -> NULL`).
- Displays each commit's ID, message, parent link, and list of tracked files.

Example:
```bash
minigit log
```
Output:
```text
commit C002
Message: Version 2 - XYZ
Parent:  C001
Files (1): test.txt

    |
    v

commit C001
Message: Version 1 - ABC
Parent:  NULL
Files (1): test.txt
```

### 7. Test Custom Hash Table
```bash
minigit test-hashtable
```
- Runs the test suite verifying:
  - Separate chaining collision handling (Bucket 0 collision between `main.cpp` and `beta.txt`).
  - Dynamic resizing: tracks table capacity before and after inserting 30 keys ($10 \to 40$) with node relinking without reallocating nodes.
  - Insert, update, lookup (`get`), presence check (`contains`), and node removal (`remove`).

---

## DSA Concepts Implemented

1. **Polynomial Rolling Hash:**
   $$\text{hash} = \sum_{i=0}^{n-1} c_i \cdot 31^{n-1-i} \pmod{2^{64}}$$
   - Multiplier `31` avoids anagram collisions; unsigned 64-bit integer overflow provides cross-platform determinism.
   - Time Complexity: $O(n)$ where $n$ is file size in bytes.

2. **Custom Hash Table with Separate Chaining & Dynamic Resizing:**
   - Dynamic bucket array `Node** buckets` starting at initial capacity 10.
   - Separate chaining with singly linked lists (`Node { string key, string value, Node* next }`).
   - Load factor threshold $\alpha = \frac{\text{elementCount}}{\text{capacity}} > 0.75$.
   - **Rehashing by Relinking:** When resizing triggers, the capacity doubles ($10 \to 20 \to 40$), and existing nodes are relinked into the new bucket array without deleting or re-allocating `Node` instances ($O(n)$ time, $0$ new Node allocations).
   - Time Complexity: $O(1)$ average lookup, insert, and delete; amortized $O(1)$ insertions.

3. **Singly Linked List of Commits:**
   - Commit history forms an explicit backward-pointing linked list (`HEAD → C003 → C001 → NULL`).
   - Traversal (`minigit log`): $O(k)$ where $k$ is commit chain length.

4. **Persistent Commit Counter (`.minigit/counter`):**
   - Keeps track of the highest assigned commit sequence number ($O(1)$).
   - Prevents commit ID collisions after branch/commit checkout.

5. **Content-Addressable Storage (CAS / Blobs):**
   - Objects are stored by content hash (`.minigit/objects/<hash>`).
   - Implements collision detection: compares content bytes if a hash exists to guarantee data integrity.

6. **State Synchronization & Set Difference:**
   - Checkout computes symmetric differences between the previous commit and target commit to prune obsolete files and restore exact snapshots.

---

## Compilation and Build Instructions

### Compilation
Using standard C++17 with `g++` and zero compiler warnings:

```bash
g++ -std=c++17 -Wall -Wextra main.cpp Hash.cpp HashTable.cpp -o minigit.exe
```

### Running Commands (Windows PowerShell / CMD)
```powershell
.\minigit.exe init
.\minigit.exe add main.cpp
.\minigit.exe commit "Initial commit"
.\minigit.exe test-hashtable
```
