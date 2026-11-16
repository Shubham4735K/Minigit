# MiniGit — Simple Version Control System using Data Structures

A lightweight command-line version-control project designed for 2nd-year Data Structures and Algorithms (DSA). MiniGit demonstrates core data structure concepts step-by-step.

## Current Project Status
**Phase 9 — Line-by-Line Diff, Longest Common Subsequence (LCS) & Commit Comparison**

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

### 3. Stage Files
```bash
minigit add <filename>
minigit add .
```
- **Single File (`add <filename>`):**
  - Checks that current directory is an initialized MiniGit repository.
  - Computes file hash using polynomial rolling hash `calculateHash()`.
  - **Collision-Safe Object Storage:** Verifies byte equality if `.minigit/objects/<hash>` already exists to detect hash collisions.
  - Stores content into Content-Addressable Storage (`.minigit/objects/<hash>`).
  - Loads staged files into custom in-memory `HashTable` (`filename → hash`).
  - Persists staged entries back to `.minigit/staging`.
- **Recursive Add (`add .`):**
  - Recursively traverses working directory and subdirectories.
  - Automatically skips `.minigit/`, `.git/`, and `*.exe` binaries.
  - Respects `.minigitignore` patterns: supports exact filenames and wildcard extension rules (e.g. `*.log`, `temp.txt`).
  - Stages all matching regular files.

### 4. Repository Status
```bash
minigit status
```
- Displays current branch or `(HEAD detached at <commitId>)`.
- Categorizes working tree and staging state using custom `HashTable` for $O(1)$ average lookups:
  - **Changes to be committed (Staged):**
    - `new file: <filename>` (present in staging, absent in parent commit).
    - `modified: <filename>` (present in staging, hash differs from parent commit).
  - **Changes not staged for commit:**
    - `modified: <filename>` (present in working tree, hash differs from staged/committed).
    - `deleted:  <filename>` (tracked in staging/committed, but missing from disk).
  - **Clean status:** Displays `"nothing to commit, working tree clean"` when all sets match.

### 5. Remove Files
```bash
minigit rm <filename>
minigit rm --cached <filename>
```
- Removes `<filename>` from the staging area (`.minigit/staging`).
- **Disk Removal:** By default, also deletes the file from working tree disk.
- **`--cached` Option:** Removes the file from staging while leaving the physical file on disk untouched.

### 6. Commit Staged Snapshot
```bash
minigit commit "<message>"
```
- Validates commit message: rejects newlines and empty/whitespace messages.
- Resolves parent commit from current `HEAD` / active branch.
- Compares staging table with parent snapshot: suppresses duplicates with `"Nothing to commit, working tree clean"`.
- Assigns unique sequential ID from `.minigit/counter` (`C001`, `C002`, ...).
- Records metadata headers: `id`, `message`, `parent`, `author` (from `.minigit/config`), and `timestamp` (`YYYY-MM-DD HH:MM:SS`).
- Creates snapshot at `.minigit/commits/<commitId>`.
- **Advances Pointer:**
  - If on a branch (`ref: refs/heads/<branch>`), writes `commitId` to `.minigit/refs/heads/<branch>`.
  - If in detached HEAD state, writes `commitId` directly to `.minigit/HEAD` and displays a warning.

### 7. Inspect Commit Details
```bash
minigit show <commitId>
```
- Displays detailed commit information:
  - Commit ID and active/parent graph link.
  - Author and commit timestamp.
  - Commit log message.
  - Complete list of tracked files and their content hashes.

### 8. User Configuration
```bash
minigit config user.name "<name>"
```
- Sets the commit author name in `.minigit/config`.
- Future commits record `<name> <<name>@minigit>`. Default author is `"Unknown <unknown@minigit>"`.

### 9. Branch Management (Phase 8)
```bash
minigit branch
minigit branch <name>
minigit branch -d <name>
```
- **List Branches (`minigit branch`):** Loads all branch references from `.minigit/refs/heads/` into custom `HashTable`, displays them alphabetically, and marks active branch with `*` (or displays `* (HEAD detached at <commitId>)`).
- **Create Branch (`minigit branch <name>`):** Creates a named pointer at `.minigit/refs/heads/<name>` pointing to the current commit.
- **Delete Branch (`minigit branch -d <name>`):** Deletes branch reference file (refuses to delete currently active branch).

### 10. Checkout & Switch Branches (Phase 7 & 8)
```bash
minigit checkout <branch|commitId> [--force]
minigit checkout -b <name> [--force]
```
- **Switch to Branch:** If target is a branch name, updates `HEAD` to `ref: refs/heads/<branch>` and restores snapshot files.
- **Switch to Commit (Detached HEAD):** If target is a commit ID, updates `HEAD` directly to `<commitId>` and restores snapshot files.
- **Create and Switch (`-b`):** Creates branch `<name>` at current commit and switches `HEAD` to it in one step.
- **Safety Checks:** Checks for uncommitted working tree or staging modifications against current snapshot; aborts unless `--force` is passed.
- **Tracked File Pruning:** Deletes files that were tracked in previous commit but are absent in target commit.

### 11. View Commit History Log
```bash
minigit log
minigit log --oneline
```
- Resolves starting commit node from active branch / `HEAD`.
- Traverses backward along parent pointers (`HEAD -> parent -> parent -> NULL`).
- Standard `log`: Displays full visual linked list nodes with commit ID, author, date, message, parent link, and tracked files.
- `--oneline`: Displays compact single-line format: `<commitId> <message>`.

### 12. File & Commit Differences (Phase 9)
```bash
minigit diff <filename>
minigit diff <commitA> <commitB>
```
- **Working Tree Diff (`diff <filename>`):**
  - Compares working file on disk against its last committed version in `.minigit/objects/<hash>`.
  - If identical, prints `"No differences"`.
  - If new (not in commit), reports `new file: <filename>` and all added lines (`+ `).
  - If deleted from disk, reports `deleted file: <filename>` and all removed lines (`- `).
  - If modified, prints unified-style diff with 2 lines of context around changes (`- ` removed, `+ ` added, `  ` unchanged).
- **Snapshot Diff (`diff <commitA> <commitB>`):**
  - Compares all files between two commit snapshots.
  - Automatically identifies added files (`new file:`), deleted files (`deleted file:`), and modified files.
  - If both snapshots are identical, prints `"No differences"`.
- **Large File Protection:**
  - If $m \times n > 25\text{ million}$ DP table cells, prints a safety warning and falls back to reporting `"Files differ"`.

### 13. Test Custom Hash Table
```bash
minigit test-hashtable
```
- Verifies custom `HashTable` operations:
  - Separate chaining collision resolution in Bucket 0 (`main.cpp` and `beta.txt`).
  - Dynamic resizing from capacity 10 to 40 across 30 elements with $O(n)$ node relinking and zero node reallocations.
  - Insert, update, get, contains, and remove.

### 14. Commit File Format (`.minigit/commits/<commitId>`)
Each commit snapshot is saved as a plain text file containing key-value metadata headers, followed by a blank line delimiter, and then tracked files with their polynomial hashes (`<filename>|<hash>`):

```text
id=C001
message=Initial commit
parent=NULL
author=Alice <alice@minigit>
timestamp=2026-11-16 19:00:00

main.cpp|182390192830192
README.md|981273918237192
```

**Backward Compatibility:**
MiniGit seamlessly handles legacy commit files from earlier phases that lack `author=` or `timestamp=` headers. When parsed by `loadSnapshot()`, `showCommit()`, or `printCommitLog()`, missing metadata defaults gracefully to author `"Unknown <unknown@minigit>"` and timestamp `"Unknown"`.

---

## DSA Concepts Implemented

1. **Fast Set Operations & Status Classification via Custom Hash Table:**
   - `minigit status` compares three distinct file sets: the **Working Directory**, the **Staging Area**, and the **HEAD Commit Snapshot**.
   - Instead of a naive $O(N_w \times N_s)$ nested loop search, all lookups are performed in $O(1)$ average time using our custom `HashTable`.
   - Overall status classification completes in linear time: $O(N_w + N_s + N_c)$.

2. **Recursive Directory Traversal with Pattern Filtering:**
   - `minigit add .` recursively explores directory sub-trees using Depth-First Search (DFS).
   - Pattern matching handles `.minigitignore` ignore rules: exact file matches and suffix/extension wildcards (`*.ext`).

3. **Named Pointers in a Directed Acyclic Graph (DAG):**
   - Branches in MiniGit are not copies of repository files; they are $O(1)$ named pointers (`.minigit/refs/heads/<name>`) pointing to nodes in the commit graph.
   - Advancing a branch upon commit is an $O(1)$ pointer update.
   - Switching branches is an $O(1)$ pointer redirect in `HEAD`.

4. **Custom Hash Table for Branch Reference Dictionaries:**
   - When listing, creating, or switching branches, MiniGit reads all branch reference files into a custom `HashTable` (`branchName → commitId`).
   - Ensures average $O(1)$ branch lookup and collision-free reference management without using `std::unordered_map`.

5. **Polynomial Rolling Hash:**
   $$\text{hash} = \sum_{i=0}^{n-1} c_i \cdot 31^{n-1-i} \pmod{2^{64}}$$
   - Multiplier `31` avoids anagram collisions; unsigned 64-bit integer overflow provides cross-platform determinism. Time Complexity: $O(n)$.

6. **Dynamic Hash Table with Separate Chaining:**
   - Dynamic bucket array `Node** buckets` starting at initial capacity 10.
   - Load factor threshold $\alpha = \frac{\text{elementCount}}{\text{capacity}} > 0.75$.
   - Rehashing relinks existing nodes into doubled bucket array without node reallocations.

7. **Singly Linked List of Commits:**
   - Commit history forms a backward-pointing singly linked list.
   - Traversal (`minigit log`): $O(k)$ where $k$ is chain length.

8. **Content-Addressable Storage (CAS / Blobs):**
   - Files are indexed and deduplicated by their content hash (`.minigit/objects/<hash>`).
   - Collision-safe byte comparison prevents corruption.

9. **Longest Common Subsequence (LCS) & Minimal Edit Script via 2D Dynamic Programming:**
   - `minigit diff` uses bottom-up Dynamic Programming to find the longest sequence of identical lines between two file versions.
   - **DP Recurrence:**
     $$\text{if } a[i-1] == b[j-1] \implies L[i][j] = L[i-1][j-1] + 1 \quad \text{else} \quad L[i][j] = \max(L[i-1][j], L[i][j-1])$$
   - **Backtracking:** Traces from $(m, n)$ down to $(0, 0)$ to reconstruct the optimal sequence of `UNCHANGED`, `ADDED`, and `REMOVED` line operations.
   - **Context Windowing:** Preserves up to 2 lines of unchanged context surrounding edits, collapsing distant unchanged segments into hunks with `...`.
   - **Complexity:** $O(m \times n)$ time and space; guards against excessive memory use with a 25-million cell threshold check.

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
