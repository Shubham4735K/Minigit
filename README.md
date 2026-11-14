# MiniGit — Simple Version Control System using Data Structures

A lightweight command-line version-control project designed for 2nd-year Data Structures and Algorithms (DSA). MiniGit demonstrates core data structure concepts step-by-step.

## Current Project Status
**Phase 7 — Checkout, Commit Log & Content-Addressable Storage**

---

## Supported Commands

### 1. Initialize Repository
```bash
minigit init
```
- Checks if a `.minigit` directory already exists to prevent overwriting existing data.
- Creates a hidden `.minigit/` directory and `.minigit/objects/` store for content blobs.
- Initializes the `HEAD` file pointing to the default branch (`main`).

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
- Saves the file's content into Content-Addressable Storage (`.minigit/objects/<hash>`) with deduplication.
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
- Checks that `.minigit/staging` exists and contains at least one file.
- Reads staged entries into the custom `HashTable`.
- Determines parent commit ID from `.minigit/HEAD` (`NULL` for first commit, or previous commit ID).
- Creates a new sequential commit ID (`C001`, `C002`, `C003`, ...).
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
minigit checkout <commitId>
```
- Verifies `.minigit` repository and target commit exist.
- Parses the snapshot file at `.minigit/commits/<commitId>`.
- Retrieves file contents from `.minigit/objects/<hash>` and overwrites working files to restore exact state.
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
- Runs the test suite verifying custom `HashTable` operations: insertion, lookup (`get`), presence check (`contains`), in-place updates, node deletion (`remove`), iteration (`getAll`), memory cleanup, and collision handling via separate chaining.

---

## Phase 5 Concept: Commits and Linked History

### What is a Commit?
A **commit** represents a permanent, immutable snapshot of the project's staged files at a specific moment in time.

```text
Staging Area (.minigit/staging)
             ↓
Commit Snapshot (.minigit/commits/C001)
             ↓
Parent Linkage (C002 -> parent=C001)
```

### Linked List of Commits
Each commit record includes a `parent` field pointing to the preceding commit ID. This establishes a **singly linked list** of project history:

```text
HEAD
  ↓
[C002] (parent=C001) ───► [C001] (parent=NULL)
```

### Commit File Format
Stored in `.minigit/commits/<commitId>`:
```text
id=C001
message=Initial commit
parent=NULL

main.cpp|5019670368770650375
README.md|17705612757555493917
```

---

## DSA Concepts Demonstrated So Far

1. **Hashing:** Generates a deterministic polynomial integer fingerprint from file contents ($O(n)$ where $n$ is file size in bytes).
2. **Hash Table:** Provides associative key-value mapping (`filename → hash`) with average $O(1)$ lookup, insertion, and updates.
3. **Separate Chaining:** Handles bucket index collisions by maintaining linked lists per bucket.
4. **Linked List:** 
   - Used inside Hash Table buckets to chain colliding nodes.
   - Used in commit history where each commit acts as a node pointing to its `parent`.
5. **Content-Addressable Storage (CAS / Blobs):** Files are indexed and stored by their content hash (`.minigit/objects/<hash>`), providing inherent data deduplication and full snapshot rollback capabilities.
6. **File Handling & Serialization:** Persists in-memory data structures across program executions.

---

## Planned Future Features (Not Yet Implemented)
- Branches (`minigit branch`)
- Diff
- Merge

---

## Compilation and Usage

### Compilation
Using standard C++17 with `g++`:

```bash
g++ -std=c++17 main.cpp Hash.cpp HashTable.cpp -o minigit.exe
```

### Running Commands (Windows PowerShell / CMD)
```powershell
.\minigit.exe init
.\minigit.exe add main.cpp
.\minigit.exe commit "Initial commit"
.\minigit.exe test-hashtable
```
