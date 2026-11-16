# MiniGit — Project Phases & Architecture Reference

This reference document tracks the development phases of **MiniGit**, detailing completed features, underlying Data Structures & Algorithms (DSA), file structures, and planned future phases.

---

## Roadmap Summary

| Phase | Title | Command / Feature | DSA Concept | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1** | Repository Initialization | `minigit init` | File/Directory I/O, Persistent Counter, Branch Reference initialization | **Completed** |
| **Phase 2** | Deterministic Hashing | `minigit hash <file>` | Polynomial Rolling Hash ($O(n)$) | **Completed** |
| **Phase 3** | Custom Hash Table & Dynamic Resizing | `minigit test-hashtable` | Separate Chaining, Dynamic Resizing & Node Relinking | **Completed** |
| **Phase 4** | Staging Area & CAS | `minigit add <file>` | Hash Table serialization, Collision-safe Content-Addressable Storage | **Completed** |
| **Phase 5** | Commit Snapshots | `minigit commit "<msg>"` | Singly Linked List of Commits, Persistent ID Counter, Branch Reference Advancing | **Completed** |
| **Phase 6** | Commit History Log | `minigit log` | Linked List Traversal ($O(k)$) from active branch / HEAD | **Completed** |
| **Phase 7** | Rollback & Object Storage | `minigit checkout <id> [--force]` | State Restoration, Uncommitted Changes Guard, Tracked File Pruning | **Completed** |
| **Phase 8** | Branches & Named Pointers | `minigit branch [-d]`, `minigit checkout [-b]` | Graph / DAG Branching, Named Pointers, Custom HashTable Reference Store | **Completed** |
| **Phase 9** | Everyday Commands & Repository Status | `minigit status`, `rm`, `add .`, `show`, `config`, `log --oneline` | $O(1)$ Hash Table Set Operations, DFS Directory Traversal, Commit Metadata Serialization | **Completed** |
| **Phase 10** | Line-by-Line Diff | `minigit diff` | Longest Common Subsequence (LCS) / Dynamic Programming | *Next up* |
| **Phase 11** | Three-Way Merge | `minigit merge <branch>` | Lowest Common Ancestor (LCA) on DAG | *Planned* |

---

## Detailed Phase Breakdown

### Phase 1: Repository Initialization (`minigit init`)
* **Status:** Completed
* **Purpose:** Sets up the root environment for tracking files without disturbing the user's workspace.
* **Key Actions:**
  * Checks for pre-existing `.minigit/` to prevent data loss.
  * Creates hidden directory `.minigit/`.
  * Creates `.minigit/objects/` for binary blob storage.
  * Creates `.minigit/refs/heads/` for branch reference pointers.
  * Creates `.minigit/HEAD` pointing symbolically to default branch `ref: refs/heads/main`.
  * Creates `.minigit/counter` initialized to `0` for globally unique commit IDs.
* **Files Involved:** `main.cpp` (`initRepository()`).

---

### Phase 2: Content Fingerprinting (`minigit hash <file>`)
* **Status:** Completed
* **Purpose:** Generates a deterministic, fixed-size numerical digest of any file's content.
* **DSA Concept:** **Polynomial Rolling Hash Function**
  $$\text{hash} = \sum_{i=0}^{n-1} c_i \cdot 31^{n-1-i} \pmod{2^{64}}$$
* **Files Involved:** `Hash.h`, `Hash.cpp`, `main.cpp` (`hashFile()`).

---

### Phase 3: In-Memory Key-Value Store (`HashTable`) & Dynamic Resizing
* **Status:** Completed
* **Purpose:** Custom dictionary (`filename → hash` and `branchName → commitId`) without using `<unordered_map>`.
* **DSA Concept:** **Hash Table with Separate Chaining & Dynamic Resizing**
  * Initial bucket array size: `INITIAL_CAPACITY = 10` (retained so small-dataset collision demonstrations remain intact).
  * Load Factor Threshold: $\alpha = \frac{\text{elementCount}}{\text{capacity}} > 0.75$.
  * **Dynamic Rehashing by Relinking:** Doubles capacity ($10 \to 20 \to 40 \dots$) and relinks existing `Node` pointers without deleting or re-allocating `Node` objects ($O(n)$ time, $0$ Node allocations).
  * `insert(key, value)`: Updates in-place if key exists; prepends to head ($O(1)$) if new.
  * `get(key)` / `contains(key)`: Traverses bucket chain ($O(1)$ average, $O(n)$ worst-case).
  * `remove(key)`: Unlinks node from singly linked list and frees dynamic memory (`delete`).
  * `getAll()`: Linear traversal of all buckets for disk persistence and branch listing.
* **Files Involved:** `HashTable.h`, `HashTable.cpp`.

---

### Phase 4: Staging Area & Collision-Safe CAS (`minigit add <file>`)
* **Status:** Completed
* **Purpose:** Prepares specific files for inclusion in the next commit snapshot.
* **Key Actions:**
  * Computes file hash using `calculateHash()`.
  * **Collision-Safe Object Store:** Verifies bitwise equality if object exists; reports collision error if bytes differ.
  * Loads existing `.minigit/staging` into in-memory `HashTable`.
  * Persists updated table back to `.minigit/staging`.
* **Files Involved:** `main.cpp` (`addFile()`).

---

### Phase 5: Commit Snapshots (`minigit commit "<msg>"`)
* **Status:** Completed
* **Purpose:** Creates a permanent, immutable record of staged files and advances the active branch pointer.
* **DSA Concept:** **Singly Linked List of Commits & Branch Pointer Advancing**
  * Resolves parent commit from `HEAD` (either active branch file or detached commit ID).
  * Generates unique sequential ID from `.minigit/counter`.
  * Writes `.minigit/commits/<commitId>`.
  * **Advances Active Reference:** If on a branch (`ref: refs/heads/<branch>`), writes `commitId` to `.minigit/refs/heads/<branch>`. If in detached HEAD state, updates `.minigit/HEAD` directly and warns.
* **Files Involved:** `main.cpp` (`createCommit()`).

---

### Phase 6: Commit History Log (`minigit log`)
* **Status:** Completed
* **Purpose:** Traverses the commit graph from current resolved commit backward to root commit.
* **DSA Concept:** **Linked List Traversal**
  * Resolves starting node from `HEAD` (or `refs/heads/<branch>`).
  * While `current != "NULL"`, prints metadata and advances `current = commit.parent`.
* **Complexity:** $O(k)$ where $k$ is the number of commits.
* **Files Involved:** `main.cpp` (`printCommitLog()`).

---

### Phase 7: Rollback & Safe Checkout (`minigit checkout <target> [--force]`)
* **Status:** Completed
* **Purpose:** Restores workspace files to match historical snapshots or branch heads.
* **Key Actions:**
  * Checks for uncommitted changes against current snapshot (aborts unless `--force` is passed).
  * Prunes files tracked in the previous commit that do not exist in the target snapshot.
  * Restores target files from Content-Addressable Storage (`.minigit/objects/<hash>`).
  * Synchronizes `.minigit/staging`.
* **Files Involved:** `main.cpp` (`checkoutTarget()`).

---

### Phase 8: Branches & Named References (`minigit branch`, `minigit checkout -b`)
* **Status:** Completed
* **Purpose:** Enables non-linear, branching development workflows.
* **DSA Concept:** **Named Pointers in a Directed Acyclic Graph (DAG) & Custom HashTable Dictionary**
  * **What is a Branch?** In Git/MiniGit, branches are simply mutable named pointers (references) stored in `.minigit/refs/heads/<branchName>`, each pointing to a commit node.
  * **HEAD Pointer:**
    * When on a branch: `ref: refs/heads/<branchName>` (symbolic reference).
    * When detached: raw commit ID (e.g. `C001`).
  * **Custom HashTable for References:** All branch files from `.minigit/refs/heads/` are loaded into our custom `HashTable` (`branchName → commitId`). Lookups and existence checks take average $O(1)$ time.
  * **Branch Operations:**
    * `minigit branch`: Lists all branches in alphabetical order, marking active branch with `*` (or showing `* (HEAD detached at <commitId>)`).
    * `minigit branch <name>`: Creates a new named pointer pointing to current commit.
    * `minigit branch -d <name>`: Deletes branch reference file (refuses to delete currently active branch).
    * `minigit checkout <branch>`: Switches symbolic HEAD to target branch and restores files.
    * `minigit checkout <commitId>`: Enters detached HEAD state, pointing HEAD directly to commit ID.
    * `minigit checkout -b <name>`: Creates a new branch at current commit and switches to it in one step.
  * **Automatic Migration:** Older repositories with `HEAD = "main"` or raw commit IDs are transparently upgraded to the modern `refs/heads/` structure.
* **Files Involved:** `main.cpp` (`listBranches()`, `createBranch()`, `deleteBranch()`, `checkoutTarget()`, `checkoutNewBranch()`, `loadBranchTable()`, `migrateOldRepository()`).

---

### Phase 9: Everyday Commands & Repository Status (`minigit status`, `rm`, `add .`, `show`, `config`, `log --oneline`)
* **Status:** Completed
* **Purpose:** Provides comprehensive daily workflow tools for repository inspection, selective staging/unstaging, user identity configuration, recursive staging, and granular commit inspection.
* **DSA Concept:** **Fast Set Operations via Custom HashTable & Recursive Directory Traversal**
  * **Repository Status (`minigit status`):**
    * Compares three distinct file universes: the Working Directory ($W$), the Staging Area ($S$), and the HEAD Commit Snapshot ($C$).
    * Employs our custom `HashTable` for fast key-value lookups ($O(1)$ average time per probe) to classify each file into:
      * **Changes to be committed (Staged):**
        * New staged files: $f \in S \land f \notin C$.
        * Modified staged files: $f \in S \land f \in C \land \text{hash}_S(f) \ne \text{hash}_C(f)$.
      * **Changes not staged for commit:**
        * Working tree modified: $f \in W \land ((f \in S \land \text{hash}_W(f) \ne \text{hash}_S(f)) \lor (f \notin S \land f \in C \land \text{hash}_W(f) \ne \text{hash}_C(f)))$.
        * Tracked deleted: $(f \in S \lor f \in C) \land f \notin W$.
    * Time Complexity: $O(N_w + N_s + N_c)$ vs. naive $O(N^2)$ comparisons.
  * **Recursive Staging (`minigit add .`):**
    * Traverses directory tree depth-first (DFS).
    * Applies `.minigitignore` pattern filtering (exact filename and `*.ext` extension patterns).
    * Bypasses internal metadata (`.minigit/`, `.git/`) and executable binaries (`*.exe`).
  * **Staging and Disk Removal (`minigit rm [--cached]`):**
    * Modifies staging table in $O(1)$ time via `table.remove(file)`.
    * Optionally unlinks working directory file from disk.
  * **Commit Metadata & Extensible Headers (`minigit show`, `minigit config`):**
    * Reads and writes `.minigit/config` (`user.name`).
    * Appends `author=<name>` and `timestamp=<YYYY-MM-DD HH:MM:SS>` headers to commit objects.
    * Backward-compatible header parsing: missing fields gracefully default to `"Unknown <unknown@minigit>"`.
  * **Compact Log (`minigit log --oneline`):**
    * Singly linked list traversal outputting compact single-line commit summaries: `<commitId> <message>`.
* **Files Involved:** `main.cpp` (`printStatus()`, `removeFile()`, `collectFilesRecursive()`, `isIgnored()`, `showCommit()`, `getUserConfig()`, `setUserConfig()`, `printCommitLog()`).

---

### Phase 10: File Difference (`minigit diff`)
* **Status:** Planned
* **Purpose:** Compares the working file against the staged or last committed version.
* **DSA Concept:** **Longest Common Subsequence (LCS) / Dynamic Programming**
  * $O(m \times n)$ 2D matrix dynamic programming to compute minimal edit script (insertions/deletions).

---

### Phase 11: Three-Way Merge (`minigit merge <branch>`)
* **Status:** Planned
* **Purpose:** Automatically merges changes from another branch into the current branch.
* **DSA Concept:** **Lowest Common Ancestor (LCA)**
  * Traverses backward through commit parents to find common ancestor node in the DAG.
  * Applies three-way reconciliation algorithm (ancestor vs base vs incoming).
