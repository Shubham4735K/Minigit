#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <system_error>
#include "Hash.h"
#include "HashTable.h"

using namespace std;

// C++17 Filesystem support with backward-compatibility layer for legacy compilers
#if defined(__has_include) && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#include <windows.h>

namespace fs {
    struct path {
        std::string p;
        path(const std::string& s = "") : p(s) {}
        std::string string() const { return p; }
        path filename() const {
            size_t pos = p.find_last_of("/\\");
            return (pos == std::string::npos) ? path(p) : path(p.substr(pos + 1));
        }
        bool has_parent_path() const {
            return p.find_last_of("/\\") != std::string::npos;
        }
        path parent_path() const {
            size_t pos = p.find_last_of("/\\");
            return (pos == std::string::npos) ? path("") : path(p.substr(0, pos));
        }
    };

    struct directory_entry {
        path entry_path;
        path path() const { return entry_path; }
    };

    struct directory_iterator {
        HANDLE hFind;
        WIN32_FIND_DATAA findData;
        directory_entry current;
        std::string dir;
        bool done;

        directory_iterator() : hFind(INVALID_HANDLE_VALUE), current(), dir(""), done(true) {}
        directory_iterator(const std::string& d, std::error_code&)
            : hFind(INVALID_HANDLE_VALUE), current(), dir(d), done(false) {
            std::string search = dir + "/*";
            hFind = FindFirstFileA(search.c_str(), &findData);
            if (hFind == INVALID_HANDLE_VALUE) {
                done = true;
            } else {
                advanceUntilValid();
            }
        }
        ~directory_iterator() {
            if (hFind != INVALID_HANDLE_VALUE) {
                FindClose(hFind);
            }
        }
        void advanceUntilValid() {
            while (!done) {
                std::string name = findData.cFileName;
                if (name != "." && name != "..") {
                    current.entry_path = path(dir + "/" + name);
                    return;
                }
                if (!FindNextFileA(hFind, &findData)) {
                    done = true;
                }
            }
        }
        directory_iterator& operator++() {
            if (!FindNextFileA(hFind, &findData)) {
                done = true;
            } else {
                advanceUntilValid();
            }
            return *this;
        }
        bool operator!=(const directory_iterator& other) const {
            return done != other.done;
        }
        const directory_entry& operator*() const { return current; }
    };

    inline directory_iterator begin(directory_iterator iter) { return iter; }
    inline directory_iterator end(const directory_iterator&) { return directory_iterator(); }

    inline bool exists(const std::string& p, std::error_code&) {
        struct stat st;
        return (stat(p.c_str(), &st) == 0);
    }
    inline bool is_directory(const std::string& p, std::error_code&) {
        struct stat st;
        return (stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR));
    }
    inline bool create_directory(const std::string& p, std::error_code&) {
        return _mkdir(p.c_str()) == 0;
    }
    inline bool create_directories(const std::string& p, std::error_code& ec) {
        if (p.empty() || exists(p, ec)) return true;
        path pt(p);
        if (pt.has_parent_path()) {
            create_directories(pt.parent_path().string(), ec);
        }
        return create_directory(p, ec) || exists(p, ec);
    }
    inline bool remove(const std::string& p, std::error_code&) {
        return ::remove(p.c_str()) == 0;
    }
}
#endif

// ============================================================================
// Shared Helper Functions (Refactoring - Task 1 Item G)
// ============================================================================

// Strips trailing carriage returns ('\r') and trailing whitespace from lines.
// Critical for cross-platform Windows (\r\n) line endings compatibility.
// Time Complexity: O(k) where k is the count of trailing whitespace characters
void trimCarriageReturn(string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
}

// Reads an entire file in binary mode into a string.
// Preserves exact bytes for deterministic hashing and bitwise object comparisons.
// Time Complexity: O(n) where n is the file size in bytes
bool readFileToString(const string& path, string& content) {
    ifstream file(path, ios::binary);
    if (!file.is_open()) {
        return false;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    return true;
}

// Parses stream lines formatted as "filename|hash" into a custom HashTable.
// Time Complexity: O(L) where L is total characters in stream
void parseKVStream(istream& in, HashTable& table) {
    string line;
    while (getline(in, line)) {
        trimCarriageReturn(line);
        if (line.empty()) continue;
        size_t delimiterPos = line.find('|');
        if (delimiterPos != string::npos) {
            string key = line.substr(0, delimiterPos);
            string val = line.substr(delimiterPos + 1);
            table.insert(key, val);
        }
    }
}

// Loads the staging table from .minigit/staging into a HashTable
// Time Complexity: O(L) where L is the staging file size
HashTable loadStagingTable() {
    HashTable table;
    const string stagingFile = ".minigit/staging";
    ifstream stagingIn(stagingFile);
    if (stagingIn.is_open()) {
        parseKVStream(stagingIn, table);
        stagingIn.close();
    }
    return table;
}

// Loads a commit snapshot's file mapping (filename -> hash) into a HashTable.
// Helper required by Task 1 Item B.
// Time Complexity: O(L) where L is the commit snapshot file size
HashTable loadSnapshot(const string& commitId) {
    HashTable table;
    if (commitId.empty() || commitId == "main" || commitId == "NULL") {
        return table;
    }

    const string commitFilePath = ".minigit/commits/" + commitId;
    ifstream commitIn(commitFilePath);
    if (!commitIn.is_open()) {
        return table;
    }

    string line;
    while (getline(commitIn, line)) {
        trimCarriageReturn(line);
        if (line.empty()) continue;

        // Skip commit metadata headers
        if (line.rfind("id=", 0) == 0 || line.rfind("parent=", 0) == 0 || line.rfind("message=", 0) == 0) {
            continue;
        }

        size_t delimiterPos = line.find('|');
        if (delimiterPos != string::npos) {
            string fName = line.substr(0, delimiterPos);
            string fHash = line.substr(delimiterPos + 1);
            table.insert(fName, fHash);
        }
    }
    commitIn.close();
    return table;
}

// Serializes all entries from a HashTable to a text file in "key|value\n" format.
// Time Complexity: O(n + capacity) where n is table element count
bool saveTableToFile(const string& filePath, const HashTable& table) {
    ofstream out(filePath);
    if (!out.is_open()) return false;
    for (const auto& entry : table.getAll()) {
        out << entry.first << "|" << entry.second << "\n";
    }
    out.close();
    return true;
}

// Reads HEAD pointer from .minigit/HEAD
// Time Complexity: O(1)
string getHeadCommit() {
    const string headFile = ".minigit/HEAD";
    ifstream headIn(headFile);
    if (!headIn.is_open()) return "";
    string headContent;
    if (getline(headIn, headContent)) {
        trimCarriageReturn(headContent);
    }
    headIn.close();
    return headContent;
}

// ============================================================================
// Persistent Counter for Commit IDs (Task 1 Item A)
// ============================================================================

// Retrieves the next commit sequence number using persistent counter file.
// If counter file is missing, scans .minigit/commits/ directory for highest commit ID.
// Time Complexity: O(1) when counter exists, O(C) directory scan on legacy repos (C = number of commits).
int getNextCommitNumber(const string& repoDir) {
    const string counterPath = repoDir + "/counter";
    const string commitsDir = repoDir + "/commits";
    int lastUsed = -1;

    // 1. Try reading existing counter file
    ifstream in(counterPath);
    if (in.is_open()) {
        string line;
        if (getline(in, line)) {
            trimCarriageReturn(line);
            try {
                lastUsed = stoi(line);
            } catch (...) {
                lastUsed = -1;
            }
        }
        in.close();
    }

    // 2. If counter is missing or invalid, rebuild by scanning .minigit/commits/
    if (lastUsed < 0) {
        lastUsed = 0;
        error_code ec;
        if (fs::exists(commitsDir, ec)) {
            for (const auto& entry : fs::directory_iterator(commitsDir, ec)) {
                string name = entry.path().filename().string();
                if (name.size() > 1 && (name[0] == 'C' || name[0] == 'c')) {
                    try {
                        int num = stoi(name.substr(1));
                        if (num > lastUsed) {
                            lastUsed = num;
                        }
                    } catch (...) {}
                }
            }
        }
    }

    int nextNum = lastUsed + 1;

    // 3. Persist the updated counter value back to .minigit/counter
    ofstream out(counterPath);
    if (out.is_open()) {
        out << nextNum << "\n";
        out.close();
    }

    return nextNum;
}

// ============================================================================
// Core MiniGit Commands
// ============================================================================

// Function to initialize a new MiniGit repository (Phase 1 + Task 1 Item A counter)
void initRepository() {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string objectsDir = repoDir + "/objects";
    const string counterFile = repoDir + "/counter";

    error_code ec;

    // 1. Check if .minigit repository directory already exists
    if (fs::exists(repoDir, ec)) {
        cout << "MiniGit repository already exists." << endl;
        return;
    }

    // 2. Create the hidden .minigit directory and objects storage
    if (!fs::create_directory(repoDir, ec)) {
        cerr << "Error: Unable to create repository directory '" << repoDir << "'." << endl;
        return;
    }

    // Create objects directory for Content-Addressable Storage (Blobs)
    fs::create_directory(objectsDir, ec);

    // 3. Create HEAD file and write default branch name 'main'
    ofstream head(headFile);
    if (!head.is_open()) {
        cerr << "Error: Unable to create HEAD file at '" << headFile << "'." << endl;
        return;
    }
    head << "main\n";
    head.close();

    // 4. Initialize persistent commit counter with 0 (Task 1 Item A)
    ofstream counterOut(counterFile);
    if (counterOut.is_open()) {
        counterOut << "0\n";
        counterOut.close();
    }

    cout << "Initialized empty MiniGit repository in " << repoDir << "/" << endl;
}

// Function to compute and display the hash of a file (Phase 2)
void hashFile(const string& filename) {
    error_code ec;

    // 1. Check if the file exists
    if (!fs::exists(filename, ec)) {
        cerr << "Error: File not found." << endl;
        return;
    }

    // 2. Check if the path is a directory instead of a regular file
    if (fs::is_directory(filename, ec)) {
        cerr << "Error: '" << filename << "' is a directory, not a file." << endl;
        return;
    }

    // 3. Read file contents
    string content;
    if (!readFileToString(filename, content)) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    // 4. Calculate polynomial rolling hash
    string hashValue = calculateHash(content);

    // 5. Display the file name and its computed hash
    cout << "File: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to stage a file in MiniGit (Phase 4 + Task 1 Item C collision safety)
void addFile(const string& filename) {
    const string repoDir = ".minigit";
    const string stagingFile = repoDir + "/staging";
    const string objectsDir = repoDir + "/objects";

    error_code ec;

    // 1. Check if .minigit repository directory exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Check if target file exists
    if (!fs::exists(filename, ec)) {
        cerr << "Error: File not found." << endl;
        return;
    }

    // 3. Ensure target path is a regular file, not a directory
    if (fs::is_directory(filename, ec)) {
        cerr << "Error: '" << filename << "' is a directory, not a file." << endl;
        return;
    }

    // 4. Read file content in binary mode
    string content;
    if (!readFileToString(filename, content)) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    // 5. Calculate polynomial hash
    string hashValue = calculateHash(content);

    // 6. Collision-safe Content-Addressable Storage (Task 1 Item C)
    if (!fs::exists(objectsDir, ec)) {
        fs::create_directories(objectsDir, ec);
    }
    string blobPath = objectsDir + "/" + hashValue;
    if (fs::exists(blobPath, ec)) {
        // Blob exists: verify that byte contents are identical
        string existingContent;
        if (readFileToString(blobPath, existingContent)) {
            if (existingContent != content) {
                cerr << "Error: Hash collision detected for object '" << hashValue
                     << "'! New content differs from existing object." << endl;
                return;
            }
        }
    } else {
        // Save new blob
        ofstream blobOut(blobPath, ios::binary);
        if (blobOut.is_open()) {
            blobOut << content;
            blobOut.close();
        } else {
            cerr << "Warning: Could not save blob to '" << blobPath << "'." << endl;
        }
    }

    // 7. In-memory HashTable to hold staging entries
    HashTable stagingTable = loadStagingTable();

    // 8. Insert or update staged file in HashTable
    stagingTable.insert(filename, hashValue);

    // 9. Persist updated HashTable back to .minigit/staging
    if (!saveTableToFile(stagingFile, stagingTable)) {
        cerr << "Error: Unable to write to staging file at '" << stagingFile << "'." << endl;
        return;
    }

    // 10. Display confirmation message
    cout << "Staged: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to create a new permanent commit snapshot (Phase 5 + Task 1 Items A, B, E)
void createCommit(const string& rawMessage) {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string stagingFile = repoDir + "/staging";
    const string commitsDir = repoDir + "/commits";

    error_code ec;

    // 1. Verify that .minigit directory exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Validate commit message (Task 1 Item E)
    if (rawMessage.find('\n') != string::npos || rawMessage.find('\r') != string::npos) {
        cerr << "Error: Commit message cannot contain newline characters." << endl;
        return;
    }

    string message = rawMessage;
    size_t first = message.find_first_not_of(" \t");
    if (first == string::npos) {
        cerr << "Error: Commit message cannot be empty or whitespace-only." << endl;
        return;
    }
    size_t last = message.find_last_not_of(" \t");
    message = message.substr(first, last - first + 1);

    // 3. Verify that .minigit/staging exists
    if (!fs::exists(stagingFile, ec)) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    // 4. Read all staged entries into custom HashTable
    HashTable commitTable = loadStagingTable();
    auto stagedEntries = commitTable.getAll();
    if (stagedEntries.empty()) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    // 5. Determine parent commit ID from HEAD pointer
    string parent = "NULL";
    string headContent = getHeadCommit();
    if (!headContent.empty() && headContent != "main" && headContent != "NULL") {
        parent = headContent;
    }

    // 6. Staging vs Parent Snapshot comparison (Task 1 Item B)
    if (parent != "NULL") {
        HashTable parentTable = loadSnapshot(parent);
        if (commitTable.size() == parentTable.size()) {
            bool identical = true;
            for (const auto& entry : stagedEntries) {
                if (parentTable.get(entry.first) != entry.second) {
                    identical = false;
                    break;
                }
            }
            if (identical) {
                cout << "Nothing to commit, working tree clean" << endl;
                return;
            }
        }
    }

    // 7. Derive unique commit ID using persistent counter (Task 1 Item A)
    int commitNumber = getNextCommitNumber(repoDir);
    char buf[16];
    snprintf(buf, sizeof(buf), "C%03d", commitNumber);
    string commitId = buf;

    // 8. Ensure .minigit/commits/ directory exists
    if (!fs::exists(commitsDir, ec)) {
        if (!fs::create_directory(commitsDir, ec)) {
            cerr << "Error: Unable to create commits directory." << endl;
            return;
        }
    }

    // 9. Write commit snapshot file (.minigit/commits/<commitId>)
    string commitFilePath = commitsDir + "/" + commitId;
    ofstream commitOut(commitFilePath);
    if (!commitOut.is_open()) {
        cerr << "Error: Unable to create commit file at '" << commitFilePath << "'." << endl;
        return;
    }

    commitOut << "id=" << commitId << "\n";
    commitOut << "message=" << message << "\n";
    commitOut << "parent=" << parent << "\n\n";

    for (const auto& entry : stagedEntries) {
        commitOut << entry.first << "|" << entry.second << "\n";
    }
    commitOut.close();

    // 10. Update HEAD pointer to this new commit
    ofstream headOut(headFile);
    if (!headOut.is_open()) {
        cerr << "Error: Unable to update HEAD reference." << endl;
        return;
    }
    headOut << commitId << "\n";
    headOut.close();

    // 11. Print commit confirmation
    cout << "[" << commitId << "] " << message << endl;
}

// Function to checkout a past commit and restore files (Phase 7 + Task 1 Item F checkout safety)
void checkoutCommit(const string& commitId, bool force) {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string stagingFile = repoDir + "/staging";
    const string commitsDir = repoDir + "/commits";
    const string objectsDir = repoDir + "/objects";
    const string commitFilePath = commitsDir + "/" + commitId;

    error_code ec;

    // 1. Verify .minigit repository exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Check if specified commit file exists
    if (!fs::exists(commitFilePath, ec)) {
        cerr << "Error: Commit '" << commitId << "' does not exist." << endl;
        return;
    }

    // 3. Checkout Safety Check: verify uncommitted changes against HEAD snapshot (Task 1 Item F)
    string currentHead = getHeadCommit();
    HashTable headSnapshot;
    if (!currentHead.empty() && currentHead != "main" && currentHead != "NULL") {
        headSnapshot = loadSnapshot(currentHead);
    }

    if (!force) {
        bool hasUncommittedChanges = false;
        HashTable currentStaging = loadStagingTable();

        // Check if staging differs from HEAD snapshot
        if (currentStaging.size() != headSnapshot.size()) {
            hasUncommittedChanges = true;
        } else {
            for (const auto& entry : currentStaging.getAll()) {
                if (headSnapshot.get(entry.first) != entry.second) {
                    hasUncommittedChanges = true;
                    break;
                }
            }
        }

        // Check if working directory files differ from HEAD snapshot
        if (!hasUncommittedChanges) {
            for (const auto& entry : headSnapshot.getAll()) {
                const string& fName = entry.first;
                const string& fHash = entry.second;
                if (!fs::exists(fName, ec)) {
                    hasUncommittedChanges = true;
                    break;
                }
                string currentContent;
                if (readFileToString(fName, currentContent)) {
                    if (calculateHash(currentContent) != fHash) {
                        hasUncommittedChanges = true;
                        break;
                    }
                } else {
                    hasUncommittedChanges = true;
                    break;
                }
            }
        }

        if (hasUncommittedChanges) {
            cerr << "Error: You have uncommitted changes. Commit or stash them before checkout, or use --force to overwrite." << endl;
            return;
        }
    }

    // 4. Load target commit snapshot and metadata
    HashTable targetSnapshot = loadSnapshot(commitId);
    string commitMessage = "";
    ifstream commitIn(commitFilePath);
    if (commitIn.is_open()) {
        string line;
        while (getline(commitIn, line)) {
            trimCarriageReturn(line);
            if (line.rfind("message=", 0) == 0) {
                commitMessage = line.substr(8);
                break;
            }
        }
        commitIn.close();
    }

    // 5. Clean up tracked files from previous commit that do not exist in target commit (Task 1 Item F)
    for (const auto& entry : headSnapshot.getAll()) {
        const string& prevFile = entry.first;
        if (!targetSnapshot.contains(prevFile)) {
            if (fs::exists(prevFile, ec)) {
                fs::remove(prevFile, ec);
            }
        }
    }

    // 6. Restore files from .minigit/objects/<hash> into working directory
    int restoredCount = 0;
    for (const auto& entry : targetSnapshot.getAll()) {
        const string& fName = entry.first;
        const string& fHash = entry.second;
        string blobPath = objectsDir + "/" + fHash;

        if (!fs::exists(blobPath, ec)) {
            cerr << "Warning: Blob for '" << fName << "' (" << fHash << ") not found in objects store. Skipping." << endl;
            continue;
        }

        string content;
        if (!readFileToString(blobPath, content)) {
            cerr << "Warning: Unable to read blob for '" << fName << "'." << endl;
            continue;
        }

        // Ensure parent directories exist
        fs::path filePath(fName);
        if (filePath.has_parent_path()) {
            fs::create_directories(filePath.parent_path().string(), ec);
        }

        ofstream fileOut(fName, ios::binary);
        if (!fileOut.is_open()) {
            cerr << "Warning: Unable to restore file '" << fName << "'." << endl;
            continue;
        }
        fileOut << content;
        fileOut.close();

        restoredCount++;
    }

    // 7. Synchronize .minigit/staging with checked-out snapshot
    saveTableToFile(stagingFile, targetSnapshot);

    // 8. Update HEAD pointer to this commit
    ofstream headOut(headFile);
    if (!headOut.is_open()) {
        cerr << "Error: Unable to update HEAD." << endl;
        return;
    }
    headOut << commitId << "\n";
    headOut.close();

    cout << "Switched to commit " << commitId;
    if (!commitMessage.empty()) {
        cout << " (\"" << commitMessage << "\")";
    }
    cout << endl;
    cout << "Restored " << restoredCount << " file(s)." << endl;
}

// Function to display commit history log (Phase 6)
void printCommitLog() {
    const string repoDir = ".minigit";
    const string commitsDir = repoDir + "/commits";

    error_code ec;

    // 1. Verify .minigit repository exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Read HEAD file to find starting commit node
    string currentCommit = getHeadCommit();
    if (currentCommit.empty() || currentCommit == "main" || currentCommit == "NULL") {
        cout << "No commits yet." << endl;
        return;
    }

    // 3. Linked list traversal: Follow parent pointers backward from HEAD
    bool first = true;
    while (!currentCommit.empty() && currentCommit != "NULL") {
        string commitFilePath = commitsDir + "/" + currentCommit;
        if (!fs::exists(commitFilePath, ec)) {
            cerr << "Error: Corrupted commit reference '" << currentCommit << "'." << endl;
            break;
        }

        ifstream commitIn(commitFilePath);
        if (!commitIn.is_open()) {
            cerr << "Error: Unable to open commit file at '" << commitFilePath << "'." << endl;
            break;
        }

        string id = currentCommit;
        string message = "";
        string parent = "NULL";
        vector<string> files;

        string line;
        while (getline(commitIn, line)) {
            trimCarriageReturn(line);
            if (line.empty()) continue;

            if (line.rfind("id=", 0) == 0) {
                id = line.substr(3);
            } else if (line.rfind("message=", 0) == 0) {
                message = line.substr(8);
            } else if (line.rfind("parent=", 0) == 0) {
                parent = line.substr(7);
            } else {
                size_t delim = line.find('|');
                if (delim != string::npos) {
                    files.push_back(line.substr(0, delim));
                }
            }
        }
        commitIn.close();

        // Print commit details with linked list arrow
        if (!first) {
            cout << "\n    |\n    v\n\n";
        }
        first = false;

        cout << "commit " << id << endl;
        cout << "Message: " << message << endl;
        cout << "Parent:  " << parent << endl;
        cout << "Files (" << files.size() << "): ";
        for (size_t i = 0; i < files.size(); ++i) {
            cout << files[i] << (i + 1 < files.size() ? ", " : "");
        }
        cout << endl;

        // Traverse to parent node in singly linked list
        currentCommit = parent;
    }
}

// Function to test the custom HashTable implementation (Phase 3 + Task 1 Item D Dynamic Resizing)
void runHashTableTests() {
    cout << "========================================" << endl;
    cout << "      Phase 3: Custom HashTable Tests   " << endl;
    cout << "========================================" << endl;

    HashTable table;

    // 1. Test Insert
    cout << "\n--- [1] Testing Insert ---" << endl;
    table.insert("main.cpp", "12345");
    table.insert("README.md", "67890");
    table.insert("test.cpp", "99999");
    cout << "Inserted: main.cpp  -> 12345 (Bucket " << table.getBucketIndex("main.cpp") << ")" << endl;
    cout << "Inserted: README.md -> 67890 (Bucket " << table.getBucketIndex("README.md") << ")" << endl;
    cout << "Inserted: test.cpp  -> 99999 (Bucket " << table.getBucketIndex("test.cpp") << ")" << endl;

    // 2. Test Get
    cout << "\n--- [2] Testing Get ---" << endl;
    cout << "get(\"main.cpp\"):  " << table.get("main.cpp") << " (Expected: 12345)" << endl;
    cout << "get(\"README.md\"): " << table.get("README.md") << " (Expected: 67890)" << endl;
    cout << "get(\"test.cpp\"):  " << table.get("test.cpp") << " (Expected: 99999)" << endl;

    // 3. Test Contains
    cout << "\n--- [3] Testing Contains ---" << endl;
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: true)" << endl;
    cout << "contains(\"abc.cpp\"):  " << (table.contains("abc.cpp") ? "true" : "false") << " (Expected: false)" << endl;

    // 4. Test Update (no duplicate entries)
    cout << "\n--- [4] Testing Update ---" << endl;
    table.insert("main.cpp", "54321");
    cout << "Updated: main.cpp -> 54321" << endl;
    cout << "get(\"main.cpp\"): " << table.get("main.cpp") << " (Expected: 54321)" << endl;

    // 5. Test Remove
    cout << "\n--- [5] Testing Remove ---" << endl;
    bool removed = table.remove("main.cpp");
    cout << "remove(\"main.cpp\"): " << (removed ? "true" : "false") << " (Expected: true)" << endl;
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: false)" << endl;
    cout << "get(\"main.cpp\"): \"" << table.get("main.cpp") << "\" (Expected: empty string)" << endl;

    // 6. Test Collision Handling (Separate Chaining)
    cout << "\n--- [6] Testing Collision Handling (Separate Chaining) ---" << endl;
    cout << "Demonstrating natural collision resolution in Bucket 0:" << endl;
    cout << "Bucket index for \"main.cpp\": " << table.getBucketIndex("main.cpp") << endl;
    cout << "Bucket index for \"beta.txt\": " << table.getBucketIndex("beta.txt") << endl;

    // Insert both keys into hash table
    table.insert("beta.txt", "22222");
    table.insert("main.cpp", "12345");
    cout << "Inserted both \"beta.txt\" and \"main.cpp\" into the table." << endl;

    // Display internal bucket layout
    cout << "\nCurrent Bucket Structure:" << endl;
    table.printTable();

    // Verify independent retrieval from same bucket chain
    cout << "\nVerifying independent retrieval from the same bucket chain:" << endl;
    cout << "get(\"main.cpp\"): " << table.get("main.cpp") << " (Expected: 12345)" << endl;
    cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << endl;
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: true)" << endl;
    cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << endl;

    // Verify getAll()
    cout << "\nTesting getAll() iteration:" << endl;
    auto allItems = table.getAll();
    for (const auto& item : allItems) {
        cout << "  " << item.first << " -> " << item.second << endl;
    }

    // Verify removing one colliding key does not affect the other in the chain
    cout << "\nRemoving \"main.cpp\" from the collision chain..." << endl;
    table.remove("main.cpp");
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: false)" << endl;
    cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << endl;
    cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << endl;

    // 7. Test Dynamic Resizing (Task 1 Item D)
    cout << "\n--- [7] Testing Dynamic Resizing (Item D) ---" << endl;
    HashTable resizeTable;
    cout << "Table size before inserting 30 keys: " << resizeTable.getTableSize() << endl;

    for (int i = 1; i <= 30; ++i) {
        resizeTable.insert("file_" + to_string(i) + ".txt", "hash_val_" + to_string(i));
    }

    cout << "Table size after inserting 30 keys:  " << resizeTable.getTableSize() << endl;
    cout << "Total items stored: " << resizeTable.size() << " (Expected: 30)" << endl;
    cout << "Sample verification get(\"file_15.txt\"): " << resizeTable.get("file_15.txt")
         << " (Expected: hash_val_15)" << endl;

    cout << "\n========================================" << endl;
    cout << "       HashTable Tests Completed!       " << endl;
    cout << "========================================" << endl;
}

// Function to print usage instructions when an unsupported command is entered
void printUsage() {
    cout << "Unknown command." << endl;
    cout << "Available commands:" << endl;
    cout << "  minigit init" << endl;
    cout << "  minigit hash <filename>" << endl;
    cout << "  minigit add <filename>" << endl;
    cout << "  minigit commit \"<message>\"" << endl;
    cout << "  minigit checkout <commitId> [--force]" << endl;
    cout << "  minigit log" << endl;
    cout << "  minigit test-hashtable" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    string command = argv[1];

    if (command == "init") {
        initRepository();
    } 
    else if (command == "hash") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit hash <filename>" << endl;
            return 1;
        }
        hashFile(argv[2]);
    } else if (command == "add") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit add <filename>" << endl;
            return 1;
        }
        addFile(argv[2]);
    } else if (command == "commit") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit commit \"<message>\"" << endl;
            return 1;
        }
        createCommit(argv[2]);
    } else if (command == "checkout") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit checkout <commitId> [--force]" << endl;
            return 1;
        }
        string targetCommit = "";
        bool force = false;
        for (int i = 2; i < argc; ++i) {
            string arg = argv[i];
            if (arg == "--force") {
                force = true;
            } else if (targetCommit.empty()) {
                targetCommit = arg;
            }
        }
        if (targetCommit.empty()) {
            cout << "Usage:" << endl;
            cout << "  minigit checkout <commitId> [--force]" << endl;
            return 1;
        }
        checkoutCommit(targetCommit, force);
    } else if (command == "log") {
        printCommitLog();
    } else if (command == "test-hashtable") {
        runHashTableTests();
    } else {
        printUsage();
        return 1;
    }

    return 0;
}
