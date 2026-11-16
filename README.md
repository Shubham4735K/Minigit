# MiniGit — Simple Version Control System using Data Structures

A lightweight command-line version-control project designed for 2nd-year Data Structures and Algorithms (DSA). MiniGit demonstrates core data structure concepts step-by-step.

## Current Project Status
**Phase 8 — Branches, Named Pointers, Detached HEAD & Dynamic Hash Table**

---

## Supported Commands

### 1. Initialize Repository
```bash
minigit init
```
- Creates hidden `.minigit/` directory, `.minigit/objects/` store for blobs, and `.minigit/refs/heads/` for branch pointers.
- Initializes `HEAD` pointing symbolically to default branch: `ref: refs/heads/main`.
- Initializes persistent commit counter file (`.minigit/counter`) to `0`.

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
- Checks that current directory is an initialized MiniGit repository.
- Computes file hash using `calculateHash()`.
- **Collision-Safe Object Storage:** Verifies byte equality if `.minigit/objects/<hash>` already exists to detect hash collisions.
- Stores content into Content-Addressable Storage (`.minigit/objects/<hash>`).
- Loads staged files into custom in-memory `HashTable` (`filename → hash`).
- Persists staged entries back to `.minigit/staging`.

### 4. Commit Staged Snapshot
```bash
minigit commit "<message>"
```
- Validates commit message: rejects newlines and empty/whitespace messages.
- Resolves parent commit from current `HEAD` / active branch.
- Compares staging table with parent snapshot: suppresses duplicates with `"Nothing to commit, working tree clean"`.
- Assigns unique sequential ID from `.minigit/counter` (`C001`, `C002`, ...).
- Creates snapshot at `.minigit/commits/<commitId>`.
- **Advances Pointer:**
  - If on a branch (`ref: refs/heads/<branch>`), writes `commitId` to `.minigit/refs/heads/<branch>`.
  - If in detached HEAD state, writes `commitId` directly to `.minigit/HEAD` and displays a warning.

### 5. Branch Management (Phase 8)
```bash
minigit branch
minigit branch <name>
minigit branch -d <name>
```
- **List Branches (`minigit branch`):** Loads all branch references from `.minigit/refs/heads/` into custom `HashTable`, displays them alphabetically, and marks active branch with `*` (or displays `* (HEAD detached at <commitId>)`).
- **Create Branch (`minigit branch <name>`):** Creates a named pointer at `.minigit/refs/heads/<name>` pointing to the current commit.
- **Delete Branch (`minigit branch -d <name>`):** Deletes branch reference file (refuses to delete currently active branch).

### 6. Checkout & Switch Branches (Phase 7 & 8)
```bash
minigit checkout <branch|commitId> [--force]
minigit checkout -b <name> [--force]
```
- **Switch to Branch:** If target is a branch name, updates `HEAD` to `ref: refs/heads/<branch>` and restores snapshot files.
- **Switch to Commit (Detached HEAD):** If target is a commit ID, updates `HEAD` directly to `<commitId>` and restores snapshot files.
- **Create and Switch (`-b`):** Creates branch `<name>` at current commit and switches `HEAD` to it in one step.
- **Safety Checks:** Checks for uncommitted working tree or staging modifications against current snapshot; aborts unless `--force` is passed.
- **Tracked File Pruning:** Deletes files that were tracked in previous commit but are absent in target commit.

### 7. View Commit History Log
```bash
minigit log
```
- Resolves starting commit node from active branch / `HEAD`.
- Traverses backward along parent pointers (`HEAD -> parent -> parent -> NULL`).
- Displays commit ID, message, parent link, and tracked files.

### 8. Test Custom Hash Table
```bash
minigit test-hashtable
```
- Verifies custom `HashTable` operations:
  - Separate chaining collision resolution in Bucket 0 (`main.cpp` and `beta.txt`).
  - Dynamic resizing from capacity 10 to 40 across 30 elements with $O(n)$ node relinking and zero node reallocations.
  - Insert, update, get, contains, and remove.

---

## DSA Concepts Implemented

1. **Named Pointers in a Directed Acyclic Graph (DAG):**
   - Branches in MiniGit are not copies of repository files; they are $O(1)$ named pointers (`.minigit/refs/heads/<name>`) pointing to nodes in the commit graph.
   - Advancing a branch upon commit is an $O(1)$ pointer update.
   - Switching branches is an $O(1)$ pointer redirect in `HEAD`.

2. **Custom Hash Table for Branch Reference Dictionaries:**
   - When listing, creating, or switching branches, MiniGit reads all branch reference files into a custom `HashTable` (`branchName → commitId`).
   - Ensures average $O(1)$ branch lookup and collision-free reference management without using `std::unordered_map`.

3. **Polynomial Rolling Hash:**
   $$\text{hash} = \sum_{i=0}^{n-1} c_i \cdot 31^{n-1-i} \pmod{2^{64}}$$
   - Multiplier `31` avoids anagram collisions; unsigned 64-bit integer overflow provides cross-platform determinism. Time Complexity: $O(n)$.

4. **Dynamic Hash Table with Separate Chaining:**
   - Dynamic bucket array `Node** buckets` starting at initial capacity 10.
   - Load factor threshold $\alpha = \frac{\text{elementCount}}{\text{capacity}} > 0.75$.
   - Rehashing relinks existing nodes into doubled bucket array without node reallocations.

5. **Singly Linked List of Commits:**
   - Commit history forms a backward-pointing singly linked list.
   - Traversal (`minigit log`): $O(k)$ where $k$ is chain length.

6. **Content-Addressable Storage (CAS / Blobs):**
   - Files are indexed and deduplicated by their content hash (`.minigit/objects/<hash>`).
   - Collision-safe byte comparison prevents corruption.

---

## Compilation and Build Instructions

### Compilation
Using standard C++17 with `g++` and zero compiler warnings:

```bash
g++ -std=c++17 -Wall -Wextra main.cpp Hash.cpp HashTable.cpp -o minigit.exe
```

### Example Workflow
```powershell
.\minigit.exe init
.\minigit.exe add main.cpp
.\minigit.exe commit "Initial commit"
.\minigit.exe branch dev
.\minigit.exe checkout dev
.\minigit.exe branch
```
