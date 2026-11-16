#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <ctime>
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
    struct filepath {
        std::string p;
        filepath(const std::string& s = "") : p(s) {}
        std::string string() const { return p; }
        filepath filename() const {
            size_t pos = p.find_last_of("/\\");
            return (pos == std::string::npos) ? filepath(p) : filepath(p.substr(pos + 1));
        }
        bool has_parent_path() const {
            return p.find_last_of("/\\") != std::string::npos;
        }
        filepath parent_path() const {
            size_t pos = p.find_last_of("/\\");
            return (pos == std::string::npos) ? filepath("") : filepath(p.substr(0, pos));
        }
    };
    typedef filepath path;

    struct directory_entry {
        filepath entry_path;
        directory_entry(const filepath& p = filepath()) : entry_path(p) {}
        filepath path() const { return entry_path; }
    };

    inline std::vector<directory_entry> directory_iterator(const std::string& d, std::error_code&) {
        std::vector<directory_entry> entries;
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((d + "/*").c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) return entries;
        do {
            std::string name = findData.cFileName;
            if (name != "." && name != "..") {
                entries.push_back(directory_entry(filepath(d + "/" + name)));
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
        return entries;
    }

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

// Strips trailing carriage returns ('\r'), newlines ('\n'), and trailing whitespace from lines.
// Critical for cross-platform Windows (\r\n) line endings compatibility.
// Time Complexity: O(k) where k is the count of trailing whitespace characters
void trimCarriageReturn(string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
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
// Ignores all header metadata lines (id=, parent=, message=, author=, timestamp=).
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

        // Skip metadata header lines
        if (line.rfind("id=", 0) == 0 || line.rfind("parent=", 0) == 0 ||
            line.rfind("message=", 0) == 0 || line.rfind("author=", 0) == 0 ||
            line.rfind("timestamp=", 0) == 0) {
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

// ============================================================================
// Phase 8: Branching & Reference Management
// ============================================================================

// Automatically migrates repositories created before Phase 8
void migrateOldRepository(const string& repoDir) {
    error_code ec;
    const string refsHeadsDir = repoDir + "/refs/heads";
    if (!fs::exists(refsHeadsDir, ec)) {
        fs::create_directories(refsHeadsDir, ec);
    }

    const string headFile = repoDir + "/HEAD";
    if (!fs::exists(headFile, ec)) return;

    string headContent;
    if (!readFileToString(headFile, headContent)) return;
    trimCarriageReturn(headContent);

    if (headContent == "main") {
        ofstream headOut(headFile);
        if (headOut.is_open()) {
            headOut << "ref: refs/heads/main\n";
            headOut.close();
        }
    } else if (headContent.rfind("ref: ", 0) != 0 && !headContent.empty()) {
        const string mainBranchFile = refsHeadsDir + "/main";
        if (!fs::exists(mainBranchFile, ec)) {
            ofstream mainOut(mainBranchFile);
            if (mainOut.is_open()) {
                mainOut << headContent << "\n";
                mainOut.close();
            }
            ofstream headOut(headFile);
            if (headOut.is_open()) {
                headOut << "ref: refs/heads/main\n";
                headOut.close();
            }
        }
    }
}

// Loads all branch references from .minigit/refs/heads/ into our custom HashTable (branchName -> commitId)
// Time Complexity: O(B * L) where B is count of branch files
HashTable loadBranchTable(const string& repoDir) {
    HashTable branchTable;
    const string refsDir = repoDir + "/refs/heads";
    error_code ec;
    if (!fs::exists(refsDir, ec)) {
        return branchTable;
    }

    for (const auto& entry : fs::directory_iterator(refsDir, ec)) {
        string branchName = entry.path().filename().string();
        string commitId;
        if (readFileToString(entry.path().string(), commitId)) {
            trimCarriageReturn(commitId);
            branchTable.insert(branchName, commitId);
        }
    }
    return branchTable;
}

// Determines the active branch name if HEAD is a symbolic reference
string getCurrentBranchName(const string& repoDir) {
    const string headFile = repoDir + "/HEAD";
    string headContent;
    if (!readFileToString(headFile, headContent)) return "";
    trimCarriageReturn(headContent);

    const string prefix = "ref: refs/heads/";
    if (headContent.rfind(prefix, 0) == 0) {
        return headContent.substr(prefix.size());
    }
    return ""; // Detached HEAD
}

// Resolves HEAD to current commit ID
string resolveCurrentCommit(const string& repoDir) {
    const string headFile = repoDir + "/HEAD";
    string headContent;
    if (!readFileToString(headFile, headContent)) return "";
    trimCarriageReturn(headContent);

    const string prefix = "ref: refs/heads/";
    if (headContent.rfind(prefix, 0) == 0) {
        string branchName = headContent.substr(prefix.size());
        string branchFile = repoDir + "/refs/heads/" + branchName;
        string commitId;
        if (readFileToString(branchFile, commitId)) {
            trimCarriageReturn(commitId);
            return commitId;
        }
        return ""; // Branch exists but has no commits yet
    }

    return headContent; // Raw commit ID (detached HEAD)
}

// ============================================================================
// Configuration & Author / Timestamp Management (Task 3 Items 4 & 5)
// ============================================================================

// Reads a config setting from .minigit/config
string getUserConfig(const string& repoDir, const string& key, const string& defaultValue) {
    const string configFile = repoDir + "/config";
    ifstream in(configFile);
    if (!in.is_open()) return defaultValue;

    string line;
    while (getline(in, line)) {
        trimCarriageReturn(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eqPos = line.find('=');
        if (eqPos != string::npos) {
            string k = line.substr(0, eqPos);
            string v = line.substr(eqPos + 1);
            trimCarriageReturn(k);
            if (k == key) {
                return v;
            }
        }
    }
    return defaultValue;
}

// Writes or updates a configuration setting in .minigit/config
void setUserConfig(const string& repoDir, const string& key, const string& value) {
    const string configFile = repoDir + "/config";
    vector<string> lines;
    bool updated = false;

    ifstream in(configFile);
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            trimCarriageReturn(line);
            size_t eqPos = line.find('=');
            if (eqPos != string::npos) {
                string k = line.substr(0, eqPos);
                trimCarriageReturn(k);
                if (k == key) {
                    lines.push_back(key + "=" + value);
                    updated = true;
                    continue;
                }
            }
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        in.close();
    }

    if (!updated) {
        lines.push_back(key + "=" + value);
    }

    ofstream out(configFile);
    if (out.is_open()) {
        for (const auto& l : lines) {
            out << l << "\n";
        }
        out.close();
    }
}

// Returns formatted current local timestamp string (YYYY-MM-DD HH:MM:SS)
string getCurrentTimestamp() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// ============================================================================
// Persistent Counter for Commit IDs
// ============================================================================

int getNextCommitNumber(const string& repoDir) {
    const string counterPath = repoDir + "/counter";
    const string commitsDir = repoDir + "/commits";
    int lastUsed = -1;

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

    ofstream out(counterPath);
    if (out.is_open()) {
        out << nextNum << "\n";
        out.close();
    }

    return nextNum;
}

// Represents the parent commit pointers for a commit in the DAG
struct CommitParents {
    string parent1 = "NULL";
    string parent2 = "NULL";
};

// Reads both primary parent and optional merge parent from a commit object file
// DSA Concept: Directed Acyclic Graph (DAG) node incoming edges
// Time Complexity: O(H) where H is the header lines count
CommitParents getCommitParents(const string& commitId) {
    CommitParents p;
    if (commitId.empty() || commitId == "NULL" || commitId == "main") return p;
    string path = ".minigit/commits/" + commitId;
    ifstream in(path);
    if (!in.is_open()) return p;
    string line;
    while (getline(in, line)) {
        trimCarriageReturn(line);
        if (line.empty()) break; // End of header section
        if (line.rfind("parent2=", 0) == 0) {
            p.parent2 = line.substr(8);
        } else if (line.rfind("parent=", 0) == 0) {
            p.parent1 = line.substr(7);
        }
    }
    return p;
}

// ============================================================================
// Ignore Rules & Recursive Collector (Task 3 Item 3)
// ============================================================================

// Checks whether a path matches ignore patterns (or built-in rules for .minigit, .git, *.exe)
bool isIgnored(const string& pathStr, const vector<string>& ignorePatterns) {
    // 1. Built-in exclusions: repository metadata directories
    if (pathStr == ".minigit" || pathStr.rfind(".minigit/", 0) == 0) return true;
    if (pathStr == ".git" || pathStr.rfind(".git/", 0) == 0) return true;

    // Built-in rule: skip *.exe binaries
    if (pathStr.size() >= 4) {
        string ext = pathStr.substr(pathStr.size() - 4);
        if (ext == ".exe" || ext == ".EXE") return true;
    }

    size_t lastSlash = pathStr.find_last_of("/\\");
    string filename = (lastSlash == string::npos) ? pathStr : pathStr.substr(lastSlash + 1);

    // 2. User patterns from .minigitignore
    for (const auto& pattern : ignorePatterns) {
        if (pattern.empty()) continue;

        // Wildcard extension: *.ext
        if (pattern.rfind("*.", 0) == 0) {
            string ext = pattern.substr(1);
            if (pathStr.size() >= ext.size() &&
                pathStr.substr(pathStr.size() - ext.size()) == ext) {
                return true;
            }
        }
        // Directory rule: dir/
        else if (pattern.back() == '/') {
            string dirName = pattern.substr(0, pattern.size() - 1);
            if (filename == dirName || pathStr.find(pattern) != string::npos) {
                return true;
            }
        }
        // Exact filename or path match
        else {
            if (filename == pattern || pathStr == pattern) {
                return true;
            }
        }
    }
    return false;
}

// Recursively collects regular files starting from currentDir, filtering out ignored entries
void collectFilesRecursive(const string& currentDir, vector<string>& collected, const vector<string>& ignorePatterns) {
    error_code ec;
    auto entries = fs::directory_iterator(currentDir, ec);

    for (const auto& entry : entries) {
        string entryPath = entry.path().string();
        for (char& c : entryPath) {
            if (c == '\\') c = '/';
        }
        if (entryPath.rfind("./", 0) == 0) {
            entryPath = entryPath.substr(2);
        }

        if (isIgnored(entryPath, ignorePatterns)) {
            continue;
        }

        if (fs::is_directory(entryPath, ec)) {
            collectFilesRecursive(entryPath, collected, ignorePatterns);
        } else {
            collected.push_back(entryPath);
        }
    }
}

// ============================================================================
// Core MiniGit Commands
// ============================================================================

// Function to initialize a new MiniGit repository
void initRepository() {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string objectsDir = repoDir + "/objects";
    const string refsHeadsDir = repoDir + "/refs/heads";
    const string counterFile = repoDir + "/counter";

    error_code ec;

    if (fs::exists(repoDir, ec)) {
        cout << "MiniGit repository already exists." << endl;
        return;
    }

    if (!fs::create_directory(repoDir, ec)) {
        cerr << "Error: Unable to create repository directory '" << repoDir << "'." << endl;
        return;
    }

    fs::create_directory(objectsDir, ec);
    fs::create_directories(refsHeadsDir, ec);

    ofstream head(headFile);
    if (!head.is_open()) {
        cerr << "Error: Unable to create HEAD file at '" << headFile << "'." << endl;
        return;
    }
    head << "ref: refs/heads/main\n";
    head.close();

    ofstream counterOut(counterFile);
    if (counterOut.is_open()) {
        counterOut << "0\n";
        counterOut.close();
    }

    cout << "Initialized empty MiniGit repository in " << repoDir << "/" << endl;
}

// Function to compute and display the hash of a file
void hashFile(const string& filename) {
    error_code ec;

    if (!fs::exists(filename, ec)) {
        cerr << "Error: File not found." << endl;
        return;
    }

    if (fs::is_directory(filename, ec)) {
        cerr << "Error: '" << filename << "' is a directory, not a file." << endl;
        return;
    }

    string content;
    if (!readFileToString(filename, content)) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    string hashValue = calculateHash(content);
    cout << "File: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to stage a single file
void addFile(const string& filename) {
    const string repoDir = ".minigit";
    const string stagingFile = repoDir + "/staging";
    const string objectsDir = repoDir + "/objects";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    if (!fs::exists(filename, ec)) {
        cerr << "Error: File not found." << endl;
        return;
    }

    if (fs::is_directory(filename, ec)) {
        cerr << "Error: '" << filename << "' is a directory, not a file." << endl;
        return;
    }

    string content;
    if (!readFileToString(filename, content)) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    string hashValue = calculateHash(content);

    if (!fs::exists(objectsDir, ec)) {
        fs::create_directories(objectsDir, ec);
    }
    string blobPath = objectsDir + "/" + hashValue;
    if (fs::exists(blobPath, ec)) {
        string existingContent;
        if (readFileToString(blobPath, existingContent)) {
            if (existingContent != content) {
                cerr << "Error: Hash collision detected for object '" << hashValue
                     << "'! New content differs from existing object." << endl;
                return;
            }
        }
    } else {
        ofstream blobOut(blobPath, ios::binary);
        if (blobOut.is_open()) {
            blobOut << content;
            blobOut.close();
        } else {
            cerr << "Warning: Could not save blob to '" << blobPath << "'." << endl;
        }
    }

    HashTable stagingTable = loadStagingTable();
    stagingTable.insert(filename, hashValue);

    if (!saveTableToFile(stagingFile, stagingTable)) {
        cerr << "Error: Unable to write to staging file at '" << stagingFile << "'." << endl;
        return;
    }

    cout << "Staged: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to stage all files recursively: minigit add . (Task 3 Item 3)
void addAll() {
    const string repoDir = ".minigit";
    const string stagingFile = repoDir + "/staging";
    const string objectsDir = repoDir + "/objects";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    // Read .minigitignore patterns
    vector<string> ignorePatterns;
    ifstream ignoreIn(".minigitignore");
    if (ignoreIn.is_open()) {
        string line;
        while (getline(ignoreIn, line)) {
            trimCarriageReturn(line);
            if (line.empty() || line[0] == '#') continue;
            ignorePatterns.push_back(line);
        }
        ignoreIn.close();
    }

    vector<string> filesToStage;
    collectFilesRecursive(".", filesToStage, ignorePatterns);
    sort(filesToStage.begin(), filesToStage.end());

    if (filesToStage.empty()) {
        cout << "No files to stage." << endl;
        return;
    }

    if (!fs::exists(objectsDir, ec)) {
        fs::create_directories(objectsDir, ec);
    }

    HashTable stagingTable = loadStagingTable();

    for (const auto& filename : filesToStage) {
        string content;
        if (!readFileToString(filename, content)) {
            cerr << "Warning: Unable to read file '" << filename << "'. Skipping." << endl;
            continue;
        }

        string hashValue = calculateHash(content);
        string blobPath = objectsDir + "/" + hashValue;

        if (fs::exists(blobPath, ec)) {
            string existingContent;
            if (readFileToString(blobPath, existingContent)) {
                if (existingContent != content) {
                    cerr << "Error: Hash collision detected for object '" << hashValue
                         << "'! File content differs. Aborting." << endl;
                    return;
                }
            }
        } else {
            ofstream blobOut(blobPath, ios::binary);
            if (blobOut.is_open()) {
                blobOut << content;
                blobOut.close();
            }
        }

        stagingTable.insert(filename, hashValue);
        cout << "Staged: " << filename << endl;
        cout << "Hash: " << hashValue << endl;
    }

    saveTableToFile(stagingFile, stagingTable);
}

// Function to remove a file from staging (and disk unless --cached is specified) (Task 3 Item 2)
void removeFile(const string& filename, bool cached) {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    HashTable stagingTable = loadStagingTable();
    string currentCommit = resolveCurrentCommit(repoDir);
    HashTable headSnapshot = loadSnapshot(currentCommit);

    if (!stagingTable.contains(filename) && !headSnapshot.contains(filename)) {
        cerr << "Error: pathspec '" << filename << "' did not match any files." << endl;
        return;
    }

    if (stagingTable.contains(filename)) {
        stagingTable.remove(filename);
        saveTableToFile(repoDir + "/staging", stagingTable);
    }

    if (!cached) {
        if (fs::exists(filename, ec)) {
            if (!fs::remove(filename, ec)) {
                cerr << "Error: Unable to remove file '" << filename << "' from disk." << endl;
                return;
            }
        }
        cout << "rm '" << filename << "'" << endl;
    } else {
        cout << "rm --cached '" << filename << "'" << endl;
    }
}

// Function to display working tree and staging status (Task 3 Item 1)
// DSA Concept: Custom HashTable for O(1) average lookup and comparison
void printStatus() {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentBranch = getCurrentBranchName(repoDir);
    string currentCommit = resolveCurrentCommit(repoDir);

    if (!currentBranch.empty()) {
        cout << "On branch " << currentBranch << endl;
    } else if (!currentCommit.empty()) {
        cout << "HEAD detached at " << currentCommit << endl;
    } else {
        cout << "On branch main (no commits yet)" << endl;
    }

    HashTable headSnapshot = loadSnapshot(currentCommit);
    HashTable stagingTable = loadStagingTable();

    // 1. Staged changes (Staging vs HEAD snapshot)
    vector<string> stagedNew;
    vector<string> stagedModified;

    for (const auto& entry : stagingTable.getAll()) {
        const string& fName = entry.first;
        const string& fHash = entry.second;

        if (!headSnapshot.contains(fName)) {
            stagedNew.push_back(fName);
        } else if (headSnapshot.get(fName) != fHash) {
            stagedModified.push_back(fName);
        }
    }
    sort(stagedNew.begin(), stagedNew.end());
    sort(stagedModified.begin(), stagedModified.end());

    // 2. Unstaged changes (Working Tree vs Staging / HEAD snapshot)
    HashTable trackedTable;
    for (const auto& entry : headSnapshot.getAll()) {
        trackedTable.insert(entry.first, entry.second);
    }
    for (const auto& entry : stagingTable.getAll()) {
        trackedTable.insert(entry.first, entry.second);
    }

    vector<string> unstagedModified;
    vector<string> unstagedDeleted;

    for (const auto& entry : trackedTable.getAll()) {
        const string& fName = entry.first;
        const string& baseHash = entry.second;

        if (!fs::exists(fName, ec)) {
            unstagedDeleted.push_back(fName);
        } else {
            string content;
            if (readFileToString(fName, content)) {
                string workHash = calculateHash(content);
                if (workHash != baseHash) {
                    unstagedModified.push_back(fName);
                }
            }
        }
    }
    sort(unstagedModified.begin(), unstagedModified.end());
    sort(unstagedDeleted.begin(), unstagedDeleted.end());

    bool hasStaged = !stagedNew.empty() || !stagedModified.empty();
    bool hasUnstaged = !unstagedModified.empty() || !unstagedDeleted.empty();

    if (hasStaged) {
        cout << "\nChanges to be committed:" << endl;
        for (const auto& f : stagedNew) {
            cout << "  new file:   " << f << endl;
        }
        for (const auto& f : stagedModified) {
            cout << "  modified:   " << f << endl;
        }
    }

    if (hasUnstaged) {
        cout << "\nChanges not staged for commit:" << endl;
        for (const auto& f : unstagedModified) {
            cout << "  modified:   " << f << endl;
        }
        for (const auto& f : unstagedDeleted) {
            cout << "  deleted:    " << f << endl;
        }
    }

    if (!hasStaged && !hasUnstaged) {
        cout << "nothing to commit, working tree clean" << endl;
    } else if (!hasStaged && hasUnstaged) {
        cout << "\nno changes added to commit (use \"minigit add\" to stage)" << endl;
    }
}

// Function to create a new permanent commit snapshot (Phase 5 + Author & Timestamp)
void createCommit(const string& rawMessage) {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string stagingFile = repoDir + "/staging";
    const string commitsDir = repoDir + "/commits";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string mergeHeadPath = repoDir + "/MERGE_HEAD";
    bool isMerge = fs::exists(mergeHeadPath, ec);
    string parent2 = "NULL";
    string mergeBranchName = "";

    if (isMerge) {
        ifstream mhIn(mergeHeadPath);
        if (mhIn.is_open()) {
            string pLine, bLine;
            if (getline(mhIn, pLine)) {
                trimCarriageReturn(pLine);
                parent2 = pLine;
            }
            if (getline(mhIn, bLine)) {
                trimCarriageReturn(bLine);
                mergeBranchName = bLine;
            }
            mhIn.close();
        }
    }

    string message = rawMessage;
    if (message.empty() && isMerge) {
        message = "Merge branch '" + (mergeBranchName.empty() ? "merge" : mergeBranchName) + "'";
    }

    if (message.find('\n') != string::npos || message.find('\r') != string::npos) {
        cerr << "Error: Commit message cannot contain newline characters." << endl;
        return;
    }

    size_t first = message.find_first_not_of(" \t");
    if (first == string::npos) {
        cerr << "Error: Commit message cannot be empty or whitespace-only." << endl;
        return;
    }
    size_t last = message.find_last_not_of(" \t");
    message = message.substr(first, last - first + 1);

    if (!fs::exists(stagingFile, ec)) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    HashTable commitTable = loadStagingTable();
    auto stagedEntries = commitTable.getAll();
    if (stagedEntries.empty()) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    string parent = resolveCurrentCommit(repoDir);
    if (parent.empty()) {
        parent = "NULL";
    }

    if (!isMerge && parent != "NULL") {
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

    int commitNumber = getNextCommitNumber(repoDir);
    char buf[16];
    snprintf(buf, sizeof(buf), "C%03d", commitNumber);
    string commitId = buf;

    if (!fs::exists(commitsDir, ec)) {
        if (!fs::create_directory(commitsDir, ec)) {
            cerr << "Error: Unable to create commits directory." << endl;
            return;
        }
    }

    // Retrieve author and generate timestamp (Task 3 Item 5)
    string author = getUserConfig(repoDir, "user.name", "Unknown <unknown@minigit>");
    string timestamp = getCurrentTimestamp();

    string commitFilePath = commitsDir + "/" + commitId;
    ofstream commitOut(commitFilePath);
    if (!commitOut.is_open()) {
        cerr << "Error: Unable to create commit file at '" << commitFilePath << "'." << endl;
        return;
    }

    commitOut << "id=" << commitId << "\n";
    commitOut << "message=" << message << "\n";
    commitOut << "parent=" << parent << "\n";
    if (isMerge && parent2 != "NULL" && !parent2.empty()) {
        commitOut << "parent2=" << parent2 << "\n";
    }
    commitOut << "author=" << author << "\n";
    commitOut << "timestamp=" << timestamp << "\n\n";

    for (const auto& entry : stagedEntries) {
        commitOut << entry.first << "|" << entry.second << "\n";
    }
    commitOut.close();

    if (isMerge) {
        fs::remove(mergeHeadPath, ec);
    }

    string currentBranch = getCurrentBranchName(repoDir);
    if (!currentBranch.empty()) {
        string branchFilePath = repoDir + "/refs/heads/" + currentBranch;
        ofstream branchOut(branchFilePath);
        if (!branchOut.is_open()) {
            cerr << "Error: Unable to update branch reference '" << currentBranch << "'." << endl;
            return;
        }
        branchOut << commitId << "\n";
        branchOut.close();
    } else {
        ofstream headOut(headFile);
        if (!headOut.is_open()) {
            cerr << "Error: Unable to update HEAD reference." << endl;
            return;
        }
        headOut << commitId << "\n";
        headOut.close();
        cout << "Warning: Committing in 'detached HEAD' state. HEAD is now at " << commitId << endl;
    }

    cout << "[" << commitId << "] " << message << endl;
}

// Function to display commit details (Task 3 Item 4)
void showCommit(const string& commitId) {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string commitFilePath = repoDir + "/commits/" + commitId;
    if (!fs::exists(commitFilePath, ec)) {
        cerr << "Error: Commit '" << commitId << "' does not exist." << endl;
        return;
    }

    ifstream commitIn(commitFilePath);
    if (!commitIn.is_open()) {
        cerr << "Error: Unable to open commit file at '" << commitFilePath << "'." << endl;
        return;
    }

    string id = commitId;
    string message = "";
    string parent = "NULL";
    string parent2 = "NULL";
    string author = "Unknown";
    string timestamp = "Unknown";
    vector<pair<string, string>> files;

    string line;
    while (getline(commitIn, line)) {
        trimCarriageReturn(line);
        if (line.empty()) continue;

        if (line.rfind("id=", 0) == 0) {
            id = line.substr(3);
        } else if (line.rfind("message=", 0) == 0) {
            message = line.substr(8);
        } else if (line.rfind("parent2=", 0) == 0) {
            parent2 = line.substr(8);
        } else if (line.rfind("parent=", 0) == 0) {
            parent = line.substr(7);
        } else if (line.rfind("author=", 0) == 0) {
            author = line.substr(7);
        } else if (line.rfind("timestamp=", 0) == 0) {
            timestamp = line.substr(10);
        } else {
            size_t delim = line.find('|');
            if (delim != string::npos) {
                files.push_back({line.substr(0, delim), line.substr(delim + 1)});
            }
        }
    }
    commitIn.close();

    cout << "commit " << id << endl;
    cout << "Author:    " << author << endl;
    cout << "Date:      " << timestamp << endl;
    cout << "Parent:    " << parent << endl;
    if (parent2 != "NULL" && !parent2.empty()) {
        cout << "Parent2:   " << parent2 << endl;
    }
    cout << endl;
    cout << "    " << message << endl;
    cout << endl;
    cout << "Tracked Files (" << files.size() << "):" << endl;
    for (const auto& file : files) {
        cout << "  " << file.first << " (Hash: " << file.second << ")" << endl;
    }
}

// Function to checkout a target (branch name or commit ID) with safety checks
void checkoutTarget(const string& target, bool force) {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string stagingFile = repoDir + "/staging";
    const string objectsDir = repoDir + "/objects";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    HashTable branchTable = loadBranchTable(repoDir);
    bool isBranch = branchTable.contains(target);
    string targetCommitId = "";

    if (isBranch) {
        targetCommitId = branchTable.get(target);
        if (targetCommitId.empty()) {
            ofstream headOut(headFile);
            headOut << "ref: refs/heads/" << target << "\n";
            headOut.close();
            cout << "Switched to branch '" << target << "'" << endl;
            return;
        }
    } else {
        string commitFilePath = repoDir + "/commits/" + target;
        if (fs::exists(commitFilePath, ec)) {
            targetCommitId = target;
        } else {
            cerr << "Error: Commit or branch '" << target << "' does not exist." << endl;
            return;
        }
    }

    string currentCommit = resolveCurrentCommit(repoDir);
    HashTable headSnapshot;
    if (!currentCommit.empty()) {
        headSnapshot = loadSnapshot(currentCommit);
    }

    if (!force) {
        bool hasUncommittedChanges = false;
        HashTable currentStaging = loadStagingTable();

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

    HashTable targetSnapshot = loadSnapshot(targetCommitId);
    string commitMessage = "";
    string commitFilePath = repoDir + "/commits/" + targetCommitId;
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

    for (const auto& entry : headSnapshot.getAll()) {
        const string& prevFile = entry.first;
        if (!targetSnapshot.contains(prevFile)) {
            if (fs::exists(prevFile, ec)) {
                fs::remove(prevFile, ec);
            }
        }
    }

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

    saveTableToFile(stagingFile, targetSnapshot);

    ofstream headOut(headFile);
    if (!headOut.is_open()) {
        cerr << "Error: Unable to update HEAD." << endl;
        return;
    }

    if (isBranch) {
        headOut << "ref: refs/heads/" << target << "\n";
        headOut.close();
        cout << "Switched to branch '" << target << "'" << endl;
    } else {
        headOut << targetCommitId << "\n";
        headOut.close();
        cout << "Note: switching to '" << targetCommitId << "' (detached HEAD)." << endl;
        cout << "Switched to commit " << targetCommitId;
        if (!commitMessage.empty()) {
            cout << " (\"" << commitMessage << "\")";
        }
        cout << endl;
    }

    cout << "Restored " << restoredCount << " file(s)." << endl;
}

// Function to create and switch to a new branch: minigit checkout -b <name>
void checkoutNewBranch(const string& name, bool force) {
    (void)force;
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty()) {
        cerr << "Error: Cannot create branch '" << name << "'; no commits yet." << endl;
        return;
    }

    HashTable branchTable = loadBranchTable(repoDir);
    if (branchTable.contains(name)) {
        cerr << "Error: Branch '" << name << "' already exists." << endl;
        return;
    }

    string branchFilePath = repoDir + "/refs/heads/" + name;
    ofstream branchOut(branchFilePath);
    if (!branchOut.is_open()) {
        cerr << "Error: Unable to create branch file." << endl;
        return;
    }
    branchOut << currentCommit << "\n";
    branchOut.close();

    ofstream headOut(headFile);
    if (!headOut.is_open()) {
        cerr << "Error: Unable to update HEAD." << endl;
        return;
    }
    headOut << "ref: refs/heads/" << name << "\n";
    headOut.close();

    cout << "Switched to a new branch '" << name << "'" << endl;
}

// Function to list all branches, marking the active branch with an asterisk (* )
void listBranches() {
    const string repoDir = ".minigit";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentBranch = getCurrentBranchName(repoDir);
    string currentCommit = resolveCurrentCommit(repoDir);

    HashTable branchTable = loadBranchTable(repoDir);
    auto allBranches = branchTable.getAll();

    if (allBranches.empty()) {
        if (!currentBranch.empty()) {
            cout << "* " << currentBranch << endl;
        }
        return;
    }

    vector<string> names;
    for (const auto& b : allBranches) {
        names.push_back(b.first);
    }
    sort(names.begin(), names.end());

    if (currentBranch.empty() && !currentCommit.empty()) {
        cout << "* (HEAD detached at " << currentCommit << ")" << endl;
    }

    for (const auto& name : names) {
        if (name == currentBranch) {
            cout << "* " << name << endl;
        } else {
            cout << "  " << name << endl;
        }
    }
}

// Function to create a new branch at current commit
void createBranch(const string& name) {
    const string repoDir = ".minigit";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty()) {
        cerr << "Error: Cannot create branch '" << name << "'; no commits yet." << endl;
        return;
    }

    HashTable branchTable = loadBranchTable(repoDir);
    if (branchTable.contains(name)) {
        cerr << "Error: Branch '" << name << "' already exists." << endl;
        return;
    }

    string branchFilePath = repoDir + "/refs/heads/" + name;
    ofstream branchOut(branchFilePath);
    if (!branchOut.is_open()) {
        cerr << "Error: Unable to create branch pointer file." << endl;
        return;
    }
    branchOut << currentCommit << "\n";
    branchOut.close();

    cout << "Created branch '" << name << "' at " << currentCommit << "." << endl;
}

// Function to delete a branch reference: minigit branch -d <name>
void deleteBranch(const string& name) {
    const string repoDir = ".minigit";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentBranch = getCurrentBranchName(repoDir);
    if (currentBranch == name) {
        cerr << "Error: Cannot delete the current branch '" << name << "'." << endl;
        return;
    }

    HashTable branchTable = loadBranchTable(repoDir);
    if (!branchTable.contains(name)) {
        cerr << "Error: Branch '" << name << "' not found." << endl;
        return;
    }

    string branchFilePath = repoDir + "/refs/heads/" + name;
    if (!fs::remove(branchFilePath, ec)) {
        cerr << "Error: Unable to delete branch '" << name << "'." << endl;
        return;
    }

    cout << "Deleted branch '" << name << "'." << endl;
}

// Function to display commit history log (Phase 6 + Phase 8 + Task 3 Item 6 --oneline)
void printCommitLog(bool oneline) {
    const string repoDir = ".minigit";
    const string commitsDir = repoDir + "/commits";

    error_code ec;
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty() || currentCommit == "main" || currentCommit == "NULL") {
        cout << "No commits yet." << endl;
        return;
    }

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
        string parent2 = "NULL";
        vector<string> files;

        string line;
        while (getline(commitIn, line)) {
            trimCarriageReturn(line);
            if (line.empty()) continue;

            if (line.rfind("id=", 0) == 0) {
                id = line.substr(3);
            } else if (line.rfind("message=", 0) == 0) {
                message = line.substr(8);
            } else if (line.rfind("parent2=", 0) == 0) {
                parent2 = line.substr(8);
            } else if (line.rfind("parent=", 0) == 0) {
                parent = line.substr(7);
            } else if (line.rfind("author=", 0) == 0 || line.rfind("timestamp=", 0) == 0) {
                continue;
            } else {
                size_t delim = line.find('|');
                if (delim != string::npos) {
                    files.push_back(line.substr(0, delim));
                }
            }
        }
        commitIn.close();

        if (oneline) {
            cout << id << " " << message << endl;
        } else {
            if (!first) {
                cout << "\n    |\n    v\n\n";
            }
            first = false;

            cout << "commit " << id << endl;
            if (parent2 != "NULL" && !parent2.empty()) {
                cout << "Merge:   " << parent << " " << parent2 << endl;
            }
            cout << "Message: " << message << endl;
            cout << "Parent:  " << parent << endl;
            if (parent2 != "NULL" && !parent2.empty()) {
                cout << "Parent2: " << parent2 << endl;
            }
            cout << "Files (" << files.size() << "): ";
            for (size_t i = 0; i < files.size(); ++i) {
                cout << files[i] << (i + 1 < files.size() ? ", " : "");
            }
            cout << endl;
        }

        currentCommit = parent;
    }
}

// ============================================================================
// Phase 9 / Task 4: Line-by-Line Diff & Longest Common Subsequence (LCS)
// ============================================================================

// Enum representing the type of difference operation
enum class DiffType {
    UNCHANGED,
    ADDED,
    REMOVED
};

// Represents an individual line diff operation in the edit script
struct DiffOp {
    DiffType type;
    string text;

    DiffOp(DiffType t, const string& txt) : type(t), text(txt) {}
};

// Splits a string into lines, normalizing both \r\n and \n line endings
// Time Complexity: O(L) where L is content length
vector<string> splitFileLines(const string& content) {
    vector<string> lines;
    if (content.empty()) return lines;

    string line;
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                lines.push_back(line);
                line.clear();
                ++i;
            } else {
                lines.push_back(line);
                line.clear();
            }
        } else if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

// Computes the Longest Common Subsequence (LCS) of two vectors of lines
// using a bottom-up dynamic programming 2D table, then backtracks to build the edit script.
// DSA Concept: Dynamic Programming (LCS) & Backtracking
// Time Complexity:  O(m * n) where m = a.size(), n = b.size()
// Space Complexity: O(m * n) for the 2D DP matrix
// Reusable for three-way merge (Phase 10 / Task 5).
vector<DiffOp> diffLines(const vector<string>& a, const vector<string>& b) {
    size_t m = a.size();
    size_t n = b.size();

    // Guard against very large files (exceeding 25 million DP table cells)
    if (static_cast<unsigned long long>(m) * n > 25000000ULL) {
        cerr << "Warning: File too large for line-by-line diff (> 25M DP table cells)." << endl;
        vector<DiffOp> fallback;
        for (const auto& line : a) fallback.push_back(DiffOp(DiffType::REMOVED, line));
        for (const auto& line : b) fallback.push_back(DiffOp(DiffType::ADDED, line));
        return fallback;
    }

    // Base cases for empty vectors
    if (m == 0 && n == 0) {
        return {};
    }
    if (m == 0) {
        vector<DiffOp> ops;
        for (const auto& line : b) ops.push_back(DiffOp(DiffType::ADDED, line));
        return ops;
    }
    if (n == 0) {
        vector<DiffOp> ops;
        for (const auto& line : a) ops.push_back(DiffOp(DiffType::REMOVED, line));
        return ops;
    }

    // Allocate (m + 1) x (n + 1) DP table initialized to 0
    // L[i][j] stores the length of LCS of prefix a[0..i-1] and b[0..j-1]
    vector<vector<int>> L(m + 1, vector<int>(n + 1, 0));

    // Bottom-up table filling
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            // DP recurrence: if a[i]==b[j] then L[i][j]=L[i-1][j-1]+1 else max(L[i-1][j], L[i][j-1])
            if (a[i - 1] == b[j - 1]) {
                L[i][j] = L[i - 1][j - 1] + 1;
            } else {
                L[i][j] = max(L[i - 1][j], L[i][j - 1]);
            }
        }
    }

    // Backtrack from (m, n) down to (0, 0) to produce the edit script
    vector<DiffOp> ops;
    size_t i = m;
    size_t j = n;

    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            ops.push_back(DiffOp(DiffType::UNCHANGED, a[i - 1]));
            --i;
            --j;
        } else if (L[i][j - 1] >= L[i - 1][j]) {
            // Line added in b
            ops.push_back(DiffOp(DiffType::ADDED, b[j - 1]));
            --j;
        } else {
            // Line removed from a
            ops.push_back(DiffOp(DiffType::REMOVED, a[i - 1]));
            --i;
        }
    }

    while (i > 0) {
        ops.push_back(DiffOp(DiffType::REMOVED, a[i - 1]));
        --i;
    }

    while (j > 0) {
        ops.push_back(DiffOp(DiffType::ADDED, b[j - 1]));
        --j;
    }

    // Reverse backwards-collected operations to obtain chronological forward sequence
    reverse(ops.begin(), ops.end());
    return ops;
}

// Prints a diff edit script with 2 lines of context around changes
// Time Complexity: O(K) where K is number of operations
void printDiffScript(const vector<DiffOp>& ops) {
    if (ops.empty()) {
        cout << "No differences" << endl;
        return;
    }

    bool hasChanges = false;
    for (const auto& op : ops) {
        if (op.type != DiffType::UNCHANGED) {
            hasChanges = true;
            break;
        }
    }

    if (!hasChanges) {
        cout << "No differences" << endl;
        return;
    }

    size_t count = ops.size();
    vector<bool> show(count, false);

    // Mark changes and up to 2 lines of context before and after
    for (size_t i = 0; i < count; ++i) {
        if (ops[i].type != DiffType::UNCHANGED) {
            show[i] = true;
            for (size_t d = 1; d <= 2; ++d) {
                if (i >= d) show[i - d] = true;
                if (i + d < count) show[i + d] = true;
            }
        }
    }

    bool skipped = false;
    for (size_t i = 0; i < count; ++i) {
        if (show[i]) {
            if (skipped) {
                cout << "..." << endl;
                skipped = false;
            }
            if (ops[i].type == DiffType::ADDED) {
                cout << "+ " << ops[i].text << endl;
            } else if (ops[i].type == DiffType::REMOVED) {
                cout << "- " << ops[i].text << endl;
            } else {
                cout << "  " << ops[i].text << endl;
            }
        } else {
            skipped = true;
        }
    }
    if (skipped) {
        cout << "..." << endl;
    }
}

// Compares the working tree file against its last committed version in objects/<hash>
// Time Complexity: O(m * n) for diff computation
void diffWorkingFile(const string& filename) {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty() || currentCommit == "NULL") {
        cerr << "Error: No commits exist to diff against." << endl;
        return;
    }

    HashTable headSnapshot = loadSnapshot(currentCommit);
    bool inSnapshot = headSnapshot.contains(filename);
    bool onDisk = fs::exists(filename, ec);

    if (!inSnapshot && !onDisk) {
        cerr << "Error: pathspec '" << filename << "' did not match any files." << endl;
        return;
    }

    if (!inSnapshot && onDisk) {
        // Newly added file not present in HEAD
        cout << "new file: " << filename << endl;
        cout << "--- /dev/null" << endl;
        cout << "+++ b/" << filename << endl;
        string workingContent;
        if (readFileToString(filename, workingContent)) {
            vector<string> lines = splitFileLines(workingContent);
            for (const auto& line : lines) {
                cout << "+ " << line << endl;
            }
        }
        return;
    }

    if (inSnapshot && !onDisk) {
        // Tracked file deleted from disk
        cout << "deleted file: " << filename << endl;
        cout << "--- a/" << filename << endl;
        cout << "+++ /dev/null" << endl;
        string committedHash = headSnapshot.get(filename);
        string committedContent;
        if (readFileToString(repoDir + "/objects/" + committedHash, committedContent)) {
            vector<string> lines = splitFileLines(committedContent);
            for (const auto& line : lines) {
                cout << "- " << line << endl;
            }
        }
        return;
    }

    // Both in snapshot and on disk: compare contents
    string committedHash = headSnapshot.get(filename);
    string committedContent;
    if (!readFileToString(repoDir + "/objects/" + committedHash, committedContent)) {
        cerr << "Error: Unable to read committed object for '" << filename << "'." << endl;
        return;
    }

    string workingContent;
    if (!readFileToString(filename, workingContent)) {
        cerr << "Error: Unable to read working file '" << filename << "'." << endl;
        return;
    }

    vector<string> linesA = splitFileLines(committedContent);
    vector<string> linesB = splitFileLines(workingContent);

    if (linesA == linesB) {
        cout << "No differences" << endl;
        return;
    }

    if (static_cast<unsigned long long>(linesA.size()) * linesB.size() > 25000000ULL) {
        cout << "diff --minigit a/" << filename << " b/" << filename << endl;
        cout << "Warning: File '" << filename << "' is too large for line-by-line diff (> 25M DP table cells)." << endl;
        cout << "Files differ." << endl;
        return;
    }

    cout << "diff --minigit a/" << filename << " b/" << filename << endl;
    cout << "--- a/" << filename << endl;
    cout << "+++ b/" << filename << endl;
    vector<DiffOp> ops = diffLines(linesA, linesB);
    printDiffScript(ops);
}

// Compares all differing files between two commit snapshots
// Time Complexity: O(F * m * n) where F is differing files count
void diffCommits(const string& commitA, const string& commitB) {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string pathA = repoDir + "/commits/" + commitA;
    if (!fs::exists(pathA, ec)) {
        cerr << "Error: Commit '" << commitA << "' does not exist." << endl;
        return;
    }

    string pathB = repoDir + "/commits/" + commitB;
    if (!fs::exists(pathB, ec)) {
        cerr << "Error: Commit '" << commitB << "' does not exist." << endl;
        return;
    }

    HashTable snapshotA = loadSnapshot(commitA);
    HashTable snapshotB = loadSnapshot(commitB);

    auto entriesA = snapshotA.getAll();
    auto entriesB = snapshotB.getAll();

    vector<string> allFiles;
    for (const auto& p : entriesA) {
        allFiles.push_back(p.first);
    }
    for (const auto& p : entriesB) {
        if (!snapshotA.contains(p.first)) {
            allFiles.push_back(p.first);
        }
    }
    sort(allFiles.begin(), allFiles.end());

    bool anyDiff = false;

    for (const auto& file : allFiles) {
        bool inA = snapshotA.contains(file);
        bool inB = snapshotB.contains(file);

        if (inA && inB) {
            string hashA = snapshotA.get(file);
            string hashB = snapshotB.get(file);

            if (hashA == hashB) continue;

            string contentA, contentB;
            readFileToString(repoDir + "/objects/" + hashA, contentA);
            readFileToString(repoDir + "/objects/" + hashB, contentB);

            vector<string> linesA = splitFileLines(contentA);
            vector<string> linesB = splitFileLines(contentB);

            if (linesA == linesB) continue;

            anyDiff = true;
            cout << "diff --minigit a/" << file << " b/" << file << endl;

            if (static_cast<unsigned long long>(linesA.size()) * linesB.size() > 25000000ULL) {
                cout << "Warning: File '" << file << "' is too large for line-by-line diff (> 25M DP table cells)." << endl;
                cout << "Files differ." << endl;
            } else {
                cout << "--- a/" << file << endl;
                cout << "+++ b/" << file << endl;
                vector<DiffOp> ops = diffLines(linesA, linesB);
                printDiffScript(ops);
            }
        } else if (inA && !inB) {
            anyDiff = true;
            string hashA = snapshotA.get(file);
            string contentA;
            readFileToString(repoDir + "/objects/" + hashA, contentA);
            vector<string> linesA = splitFileLines(contentA);

            cout << "deleted file: " << file << endl;
            cout << "--- a/" << file << endl;
            cout << "+++ /dev/null" << endl;
            for (const auto& line : linesA) {
                cout << "- " << line << endl;
            }
        } else if (!inA && inB) {
            anyDiff = true;
            string hashB = snapshotB.get(file);
            string contentB;
            readFileToString(repoDir + "/objects/" + hashB, contentB);
            vector<string> linesB = splitFileLines(contentB);

            cout << "new file: " << file << endl;
            cout << "--- /dev/null" << endl;
            cout << "+++ b/" << file << endl;
            for (const auto& line : linesB) {
                cout << "+ " << line << endl;
            }
        }
    }

    if (!anyDiff) {
        cout << "No differences" << endl;
    }
}

// ============================================================================
// Phase 10 / Task 5: Three-Way Merge, Lowest Common Ancestor (LCA) & DAG Graph
// ============================================================================

struct DiffChunk {
    int start = 0; // line index in base (0 .. m)
    int len = 0;   // lines in base replaced (0 for pure insertion)
    vector<string> newLines;
};

// Extracts contiguous replacement/insertion/deletion chunks from diff edit script
// Time Complexity: O(K) where K is diff operations count
vector<DiffChunk> extractChunks(const vector<DiffOp>& ops) {
    vector<DiffChunk> chunks;
    int baseIdx = 0;
    size_t i = 0;
    while (i < ops.size()) {
        if (ops[i].type == DiffType::UNCHANGED) {
            baseIdx++;
            i++;
        } else {
            DiffChunk chunk;
            chunk.start = baseIdx;
            chunk.len = 0;
            while (i < ops.size() && ops[i].type != DiffType::UNCHANGED) {
                if (ops[i].type == DiffType::REMOVED) {
                    chunk.len++;
                    baseIdx++;
                } else if (ops[i].type == DiffType::ADDED) {
                    chunk.newLines.push_back(ops[i].text);
                }
                i++;
            }
            chunks.push_back(chunk);
        }
    }
    return chunks;
}

// Merges contentBase, contentOurs, contentTheirs using 3-way line reconciliation
// Writes conflict markers (<<<<<<< ours / ======= / >>>>>>> theirs) if lines clash
// Time Complexity: O(m * n) for diffLines + O(m) for chunk reconciliation
bool mergeFileLines(const string& contentBase,
                    const string& contentOurs,
                    const string& contentTheirs,
                    string& mergedOutput) {
    vector<string> baseLines = splitFileLines(contentBase);
    vector<string> oursLines = splitFileLines(contentOurs);
    vector<string> theirsLines = splitFileLines(contentTheirs);

    // If ours and theirs are identical: clean merge
    if (oursLines == theirsLines) {
        mergedOutput = contentOurs;
        return true;
    }

    // If ours is identical to base: take theirs
    if (baseLines == oursLines) {
        mergedOutput = contentTheirs;
        return true;
    }

    // If theirs is identical to base: take ours
    if (baseLines == theirsLines) {
        mergedOutput = contentOurs;
        return true;
    }

    // If base is empty (file newly added on both sides differently)
    if (baseLines.empty()) {
        ostringstream oss;
        oss << "<<<<<<< ours\n";
        for (const auto& l : oursLines) oss << l << "\n";
        oss << "=======\n";
        for (const auto& l : theirsLines) oss << l << "\n";
        oss << ">>>>>>> theirs\n";
        mergedOutput = oss.str();
        return false;
    }

    vector<DiffChunk> chunksO = extractChunks(diffLines(baseLines, oursLines));
    vector<DiffChunk> chunksT = extractChunks(diffLines(baseLines, theirsLines));

    int cur = 0;
    size_t iO = 0;
    size_t iT = 0;
    bool hasConflict = false;
    vector<string> resultLines;

    while (iO < chunksO.size() || iT < chunksT.size()) {
        if (iO < chunksO.size() && iT < chunksT.size()) {
            int sO = chunksO[iO].start;
            int eO = chunksO[iO].start + chunksO[iO].len;
            int sT = chunksT[iT].start;
            int eT = chunksT[iT].start + chunksT[iT].len;

            // Check non-overlapping cases
            if (eO < sT || (eO == sT && (chunksO[iO].len > 0 || chunksT[iT].len > 0))) {
                while (cur < sO) {
                    resultLines.push_back(baseLines[cur++]);
                }
                for (const auto& l : chunksO[iO].newLines) {
                    resultLines.push_back(l);
                }
                cur = eO;
                iO++;
            } else if (eT < sO || (eT == sO && (chunksO[iO].len > 0 || chunksT[iT].len > 0))) {
                while (cur < sT) {
                    resultLines.push_back(baseLines[cur++]);
                }
                for (const auto& l : chunksT[iT].newLines) {
                    resultLines.push_back(l);
                }
                cur = eT;
                iT++;
            } else {
                // Overlapping region
                int regStart = min(sO, sT);
                while (cur < regStart) {
                    resultLines.push_back(baseLines[cur++]);
                }
                int regEnd = max(eO, eT);

                // Collect chained overlapping chunks
                size_t oStart = iO, tStart = iT;
                bool expanded = true;
                while (expanded) {
                    expanded = false;
                    if (iO + 1 < chunksO.size() && chunksO[iO + 1].start <= regEnd) {
                        iO++;
                        regEnd = max(regEnd, chunksO[iO].start + chunksO[iO].len);
                        expanded = true;
                    }
                    if (iT + 1 < chunksT.size() && chunksT[iT + 1].start <= regEnd) {
                        iT++;
                        regEnd = max(regEnd, chunksT[iT].start + chunksT[iT].len);
                        expanded = true;
                    }
                }

                // Reconstruct ours version in [regStart, regEnd)
                vector<string> oReg;
                int trackO = regStart;
                for (size_t k = oStart; k <= iO; ++k) {
                    while (trackO < chunksO[k].start) {
                        oReg.push_back(baseLines[trackO++]);
                    }
                    for (const auto& l : chunksO[k].newLines) oReg.push_back(l);
                    trackO = chunksO[k].start + chunksO[k].len;
                }
                while (trackO < regEnd) oReg.push_back(baseLines[trackO++]);

                // Reconstruct theirs version in [regStart, regEnd)
                vector<string> tReg;
                int trackT = regStart;
                for (size_t k = tStart; k <= iT; ++k) {
                    while (trackT < chunksT[k].start) {
                        tReg.push_back(baseLines[trackT++]);
                    }
                    for (const auto& l : chunksT[k].newLines) tReg.push_back(l);
                    trackT = chunksT[k].start + chunksT[k].len;
                }
                while (trackT < regEnd) tReg.push_back(baseLines[trackT++]);

                if (oReg == tReg) {
                    for (const auto& l : oReg) resultLines.push_back(l);
                } else {
                    hasConflict = true;
                    resultLines.push_back("<<<<<<< ours");
                    for (const auto& l : oReg) resultLines.push_back(l);
                    resultLines.push_back("=======");
                    for (const auto& l : tReg) resultLines.push_back(l);
                    resultLines.push_back(">>>>>>> theirs");
                }

                cur = regEnd;
                iO++;
                iT++;
            }
        } else if (iO < chunksO.size()) {
            int sO = chunksO[iO].start;
            int eO = chunksO[iO].start + chunksO[iO].len;
            while (cur < sO) {
                resultLines.push_back(baseLines[cur++]);
            }
            for (const auto& l : chunksO[iO].newLines) {
                resultLines.push_back(l);
            }
            cur = eO;
            iO++;
        } else {
            int sT = chunksT[iT].start;
            int eT = chunksT[iT].start + chunksT[iT].len;
            while (cur < sT) {
                resultLines.push_back(baseLines[cur++]);
            }
            for (const auto& l : chunksT[iT].newLines) {
                resultLines.push_back(l);
            }
            cur = eT;
            iT++;
        }
    }

    while (cur < static_cast<int>(baseLines.size())) {
        resultLines.push_back(baseLines[cur++]);
    }

    ostringstream oss;
    for (const auto& l : resultLines) {
        oss << l << "\n";
    }
    mergedOutput = oss.str();
    return !hasConflict;
}

// Finds the lowest common ancestor (merge base) between two commits in the DAG
// DSA Concept: Lowest Common Ancestor (LCA) in a Directed Acyclic Graph (DAG)
// History is a DAG, not just a list, once merges exist (commits can have two parents).
// Uses BFS (std::queue) from commit A collecting all ancestors into a custom HashTable (set),
// then BFS from commit B to find the first common ancestor node.
// Time Complexity:  O(V + E) where V = commits, E = parent references
// Space Complexity: O(V) for queue and ancestor HashTable
string findMergeBase(const string& commitA, const string& commitB) {
    if (commitA.empty() || commitB.empty() || commitA == "NULL" || commitB == "NULL") {
        return "";
    }
    if (commitA == commitB) {
        return commitA;
    }

    // 1. BFS from commit A collecting all reachable ancestors into HashTable
    HashTable ancestorsA;
    queue<string> qA;
    qA.push(commitA);
    ancestorsA.insert(commitA, "1");

    while (!qA.empty()) {
        string curr = qA.front();
        qA.pop();

        CommitParents cp = getCommitParents(curr);
        if (cp.parent1 != "NULL" && !cp.parent1.empty() && !ancestorsA.contains(cp.parent1)) {
            ancestorsA.insert(cp.parent1, "1");
            qA.push(cp.parent1);
        }
        if (cp.parent2 != "NULL" && !cp.parent2.empty() && !ancestorsA.contains(cp.parent2)) {
            ancestorsA.insert(cp.parent2, "1");
            qA.push(cp.parent2);
        }
    }

    // 2. BFS from commit B to locate the nearest ancestor that was also reachable from A
    queue<string> qB;
    HashTable visitedB;
    qB.push(commitB);
    visitedB.insert(commitB, "1");

    while (!qB.empty()) {
        string curr = qB.front();
        qB.pop();

        if (ancestorsA.contains(curr)) {
            return curr;
        }

        CommitParents cp = getCommitParents(curr);
        if (cp.parent1 != "NULL" && !cp.parent1.empty() && !visitedB.contains(cp.parent1)) {
            visitedB.insert(cp.parent1, "1");
            qB.push(cp.parent1);
        }
        if (cp.parent2 != "NULL" && !cp.parent2.empty() && !visitedB.contains(cp.parent2)) {
            visitedB.insert(cp.parent2, "1");
            qB.push(cp.parent2);
        }
    }

    return "";
}

// Performs three-way branch merge: minigit merge <targetBranch>
// Supports fast-forward, already-up-to-date, clean 3-way merge, and conflict detection
// Time Complexity: O(F * m * n) for 3-way file reconciliation
void mergeBranch(const string& targetBranch) {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string mergeHeadPath = repoDir + "/MERGE_HEAD";
    if (fs::exists(mergeHeadPath, ec)) {
        cerr << "Error: A merge is already in progress. Please commit or use 'minigit merge --abort'." << endl;
        return;
    }

    string currentBranch = getCurrentBranchName(repoDir);
    if (currentBranch.empty()) {
        cerr << "Error: Cannot merge in detached HEAD state." << endl;
        return;
    }

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty() || currentCommit == "NULL") {
        cerr << "Error: No commits in current branch to merge with." << endl;
        return;
    }

    HashTable branchTable = loadBranchTable(repoDir);
    if (!branchTable.contains(targetBranch)) {
        cerr << "Error: Branch '" << targetBranch << "' not found." << endl;
        return;
    }

    if (targetBranch == currentBranch) {
        cout << "Already up to date." << endl;
        return;
    }

    string targetCommit = branchTable.get(targetBranch);
    if (targetCommit.empty() || targetCommit == "NULL") {
        cerr << "Error: Target branch '" << targetBranch << "' has no commits." << endl;
        return;
    }

    string mergeBase = findMergeBase(currentCommit, targetCommit);
    if (mergeBase.empty()) {
        cerr << "Error: No common ancestor found between " << currentCommit << " and " << targetCommit << "." << endl;
        return;
    }

    if (mergeBase == targetCommit) {
        cout << "Already up to date." << endl;
        return;
    }

    if (mergeBase == currentCommit) {
        // Fast-forward merge
        cout << "Updating " << currentCommit << ".." << targetCommit << endl;
        cout << "Fast-forward" << endl;

        string branchFile = repoDir + "/refs/heads/" + currentBranch;
        ofstream bOut(branchFile);
        if (bOut.is_open()) {
            bOut << targetCommit << "\n";
            bOut.close();
        }

        // Restore files from target commit
        HashTable targetSnapshot = loadSnapshot(targetCommit);
        HashTable currentSnapshot = loadSnapshot(currentCommit);

        for (const auto& entry : currentSnapshot.getAll()) {
            if (!targetSnapshot.contains(entry.first)) {
                if (fs::exists(entry.first, ec)) {
                    fs::remove(entry.first, ec);
                }
            }
        }

        for (const auto& entry : targetSnapshot.getAll()) {
            string blobPath = repoDir + "/objects/" + entry.second;
            string content;
            if (readFileToString(blobPath, content)) {
                fs::path fp(entry.first);
                if (fp.has_parent_path()) {
                    fs::create_directories(fp.parent_path().string(), ec);
                }
                ofstream out(entry.first, ios::binary);
                if (out.is_open()) {
                    out << content;
                    out.close();
                }
            }
        }

        saveTableToFile(repoDir + "/staging", targetSnapshot);
        return;
    }

    // Three-Way Merge
    HashTable baseSnapshot = loadSnapshot(mergeBase);
    HashTable oursSnapshot = loadSnapshot(currentCommit);
    HashTable theirsSnapshot = loadSnapshot(targetCommit);

    vector<string> allFiles;
    for (const auto& p : baseSnapshot.getAll()) allFiles.push_back(p.first);
    for (const auto& p : oursSnapshot.getAll()) {
        if (!baseSnapshot.contains(p.first)) allFiles.push_back(p.first);
    }
    for (const auto& p : theirsSnapshot.getAll()) {
        if (!baseSnapshot.contains(p.first) && !oursSnapshot.contains(p.first)) {
            allFiles.push_back(p.first);
        }
    }
    sort(allFiles.begin(), allFiles.end());

    HashTable mergedTable;
    bool hasConflict = false;

    for (const auto& file : allFiles) {
        bool inBase = baseSnapshot.contains(file);
        bool inOurs = oursSnapshot.contains(file);
        bool inTheirs = theirsSnapshot.contains(file);

        string hashBase = inBase ? baseSnapshot.get(file) : "";
        string hashOurs = inOurs ? oursSnapshot.get(file) : "";
        string hashTheirs = inTheirs ? theirsSnapshot.get(file) : "";

        if (inOurs && inTheirs && hashOurs == hashTheirs) {
            // Same in ours and theirs -> keep
            mergedTable.insert(file, hashOurs);
        } else if (inBase && inOurs && inTheirs) {
            if (hashOurs == hashBase && hashTheirs != hashBase) {
                // Changed only in theirs -> take theirs
                mergedTable.insert(file, hashTheirs);
                string contentTheirs;
                readFileToString(repoDir + "/objects/" + hashTheirs, contentTheirs);
                ofstream out(file, ios::binary);
                if (out.is_open()) { out << contentTheirs; out.close(); }
            } else if (hashTheirs == hashBase && hashOurs != hashBase) {
                // Changed only in ours -> keep ours
                mergedTable.insert(file, hashOurs);
            } else {
                // Changed on both sides differently
                string contentBase, contentOurs, contentTheirs;
                readFileToString(repoDir + "/objects/" + hashBase, contentBase);
                readFileToString(repoDir + "/objects/" + hashOurs, contentOurs);
                readFileToString(repoDir + "/objects/" + hashTheirs, contentTheirs);

                string mergedContent;
                bool clean = mergeFileLines(contentBase, contentOurs, contentTheirs, mergedContent);
                ofstream out(file, ios::binary);
                if (out.is_open()) { out << mergedContent; out.close(); }

                if (clean) {
                    string newHash = calculateHash(mergedContent);
                    string objPath = repoDir + "/objects/" + newHash;
                    if (!fs::exists(objPath, ec)) {
                        ofstream blobOut(objPath, ios::binary);
                        if (blobOut.is_open()) { blobOut << mergedContent; blobOut.close(); }
                    }
                    mergedTable.insert(file, newHash);
                } else {
                    hasConflict = true;
                    cout << "CONFLICT (content): Merge conflict in " << file << endl;
                }
            }
        } else if (!inBase && inOurs && !inTheirs) {
            // Added only in ours -> keep ours
            mergedTable.insert(file, hashOurs);
        } else if (!inBase && !inOurs && inTheirs) {
            // Added only in theirs -> take theirs
            mergedTable.insert(file, hashTheirs);
            string contentTheirs;
            readFileToString(repoDir + "/objects/" + hashTheirs, contentTheirs);
            fs::path fp(file);
            if (fp.has_parent_path()) {
                fs::create_directories(fp.parent_path().string(), ec);
            }
            ofstream out(file, ios::binary);
            if (out.is_open()) { out << contentTheirs; out.close(); }
        } else if (!inBase && inOurs && inTheirs) {
            // Added on both sides
            if (hashOurs == hashTheirs) {
                mergedTable.insert(file, hashOurs);
            } else {
                string contentOurs, contentTheirs;
                readFileToString(repoDir + "/objects/" + hashOurs, contentOurs);
                readFileToString(repoDir + "/objects/" + hashTheirs, contentTheirs);
                string mergedContent;
                bool clean = mergeFileLines("", contentOurs, contentTheirs, mergedContent);
                ofstream out(file, ios::binary);
                if (out.is_open()) { out << mergedContent; out.close(); }
                if (clean) {
                    string newHash = calculateHash(mergedContent);
                    string objPath = repoDir + "/objects/" + newHash;
                    if (!fs::exists(objPath, ec)) {
                        ofstream blobOut(objPath, ios::binary);
                        if (blobOut.is_open()) { blobOut << mergedContent; blobOut.close(); }
                    }
                    mergedTable.insert(file, newHash);
                } else {
                    hasConflict = true;
                    cout << "CONFLICT (add/add): Merge conflict in " << file << endl;
                }
            }
        } else if (inBase && inOurs && !inTheirs) {
            if (hashOurs == hashBase) {
                // Deleted in theirs, unchanged in ours -> apply deletion
                if (fs::exists(file, ec)) {
                    fs::remove(file, ec);
                }
            } else {
                hasConflict = true;
                cout << "CONFLICT (modify/delete): " << file << " modified in ours and deleted in theirs." << endl;
            }
        } else if (inBase && !inOurs && inTheirs) {
            if (hashTheirs == hashBase) {
                // Deleted in ours, unchanged in theirs -> keep deleted
            } else {
                hasConflict = true;
                cout << "CONFLICT (modify/delete): " << file << " deleted in ours and modified in theirs." << endl;
                string contentTheirs;
                readFileToString(repoDir + "/objects/" + hashTheirs, contentTheirs);
                ofstream out(file, ios::binary);
                if (out.is_open()) { out << contentTheirs; out.close(); }
            }
        }
    }

    if (hasConflict) {
        // Record merge state in .minigit/MERGE_HEAD
        ofstream mhOut(mergeHeadPath);
        if (mhOut.is_open()) {
            mhOut << targetCommit << "\n" << targetBranch << "\n";
            mhOut.close();
        }
        cout << "Automatic merge failed; fix conflicts and then commit the result." << endl;
    } else {
        // Auto-create merge commit
        saveTableToFile(repoDir + "/staging", mergedTable);

        int nextNum = getNextCommitNumber(repoDir);
        char buf[16];
        snprintf(buf, sizeof(buf), "C%03d", nextNum);
        string newCommitId = buf;

        string author = getUserConfig(repoDir, "user.name", "Unknown <unknown@minigit>");
        string timestamp = getCurrentTimestamp();

        string commitPath = repoDir + "/commits/" + newCommitId;
        ofstream cOut(commitPath);
        if (cOut.is_open()) {
            cOut << "id=" << newCommitId << "\n";
            cOut << "message=Merge branch '" << targetBranch << "'\n";
            cOut << "parent=" << currentCommit << "\n";
            cOut << "parent2=" << targetCommit << "\n";
            cOut << "author=" << author << "\n";
            cOut << "timestamp=" << timestamp << "\n\n";

            for (const auto& entry : mergedTable.getAll()) {
                cOut << entry.first << "|" << entry.second << "\n";
            }
            cOut.close();
        }

        string branchFile = repoDir + "/refs/heads/" + currentBranch;
        ofstream bOut(branchFile);
        if (bOut.is_open()) {
            bOut << newCommitId << "\n";
            bOut.close();
        }

        cout << "Merge made by the 'three-way' strategy." << endl;
        cout << "[" << newCommitId << "] Merge branch '" << targetBranch << "'" << endl;
    }
}

// Aborts an in-progress merge conflict, restoring the working tree and staging area
// to the snapshot of HEAD, and removing .minigit/MERGE_HEAD
void abortMerge() {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string mergeHeadPath = repoDir + "/MERGE_HEAD";
    if (!fs::exists(mergeHeadPath, ec)) {
        cerr << "Error: There is no merge to abort (MERGE_HEAD missing)." << endl;
        return;
    }

    string currentCommit = resolveCurrentCommit(repoDir);
    if (currentCommit.empty() || currentCommit == "NULL") {
        cerr << "Error: Unable to resolve HEAD to abort merge." << endl;
        return;
    }

    HashTable headSnapshot = loadSnapshot(currentCommit);

    // Restore files from HEAD snapshot
    for (const auto& entry : headSnapshot.getAll()) {
        const string& filename = entry.first;
        const string& hash = entry.second;
        string blobPath = repoDir + "/objects/" + hash;
        string content;
        if (readFileToString(blobPath, content)) {
            fs::path filePath(filename);
            if (filePath.has_parent_path()) {
                fs::create_directories(filePath.parent_path().string(), ec);
            }
            ofstream out(filename, ios::binary);
            if (out.is_open()) {
                out << content;
                out.close();
            }
        }
    }

    // Reset staging area to HEAD snapshot
    saveTableToFile(repoDir + "/staging", headSnapshot);

    // Remove MERGE_HEAD
    fs::remove(mergeHeadPath, ec);

    cout << "Merge aborted." << endl;
}

// Prints an ASCII graph of the commit DAG using Kahn's topological sort and DFS/BFS
// DSA Concept: Topological Sort (Kahn's Algorithm) and DAG ASCII layout
// Time Complexity: O(V + E) where V is commits count, E is parent links
void printCommitGraph() {
    const string repoDir = ".minigit";
    error_code ec;

    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    migrateOldRepository(repoDir);

    string headBranch = getCurrentBranchName(repoDir);
    string headCommit = resolveCurrentCommit(repoDir);
    HashTable branchTable = loadBranchTable(repoDir);

    if (headCommit.empty() || headCommit == "NULL" || headCommit == "main") {
        cout << "No commits yet." << endl;
        return;
    }

    // 1. Collect all reachable commits in the DAG
    HashTable reachable;
    queue<string> q;

    if (!headCommit.empty() && headCommit != "NULL") {
        reachable.insert(headCommit, "1");
        q.push(headCommit);
    }

    for (const auto& b : branchTable.getAll()) {
        if (!b.second.empty() && b.second != "NULL" && !reachable.contains(b.second)) {
            reachable.insert(b.second, "1");
            q.push(b.second);
        }
    }

    while (!q.empty()) {
        string curr = q.front();
        q.pop();

        CommitParents cp = getCommitParents(curr);
        if (cp.parent1 != "NULL" && !cp.parent1.empty() && !reachable.contains(cp.parent1)) {
            reachable.insert(cp.parent1, "1");
            q.push(cp.parent1);
        }
        if (cp.parent2 != "NULL" && !cp.parent2.empty() && !reachable.contains(cp.parent2)) {
            reachable.insert(cp.parent2, "1");
            q.push(cp.parent2);
        }
    }

    auto allCommitsList = reachable.getAll();
    if (allCommitsList.empty()) {
        cout << "No commits yet." << endl;
        return;
    }

    // 2. Build in-memory metadata for each commit node
    struct NodeMeta {
        string id;
        string message;
        string parent1;
        string parent2;
        vector<string> refs;
        int inDegree = 0;
    };

    vector<NodeMeta> nodes;
    for (const auto& entry : allCommitsList) {
        string cid = entry.first;
        NodeMeta nm;
        nm.id = cid;
        nm.parent1 = "NULL";
        nm.parent2 = "NULL";

        string cPath = repoDir + "/commits/" + cid;
        ifstream in(cPath);
        if (in.is_open()) {
            string line;
            while (getline(in, line)) {
                trimCarriageReturn(line);
                if (line.empty()) break;
                if (line.rfind("message=", 0) == 0) {
                    nm.message = line.substr(8);
                } else if (line.rfind("parent2=", 0) == 0) {
                    nm.parent2 = line.substr(8);
                } else if (line.rfind("parent=", 0) == 0) {
                    nm.parent1 = line.substr(7);
                }
            }
            in.close();
        }

        // Attach refs
        if (cid == headCommit) {
            if (!headBranch.empty()) {
                nm.refs.push_back("HEAD -> " + headBranch);
            } else {
                nm.refs.push_back("HEAD");
            }
        }
        for (const auto& b : branchTable.getAll()) {
            if (b.second == cid) {
                if (b.first != headBranch) {
                    nm.refs.push_back(b.first);
                }
            }
        }

        nodes.push_back(nm);
    }

    // 3. Compute in-degrees (child count)
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].parent1 != "NULL" && !nodes[i].parent1.empty()) {
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (nodes[j].id == nodes[i].parent1) {
                    nodes[j].inDegree++;
                    break;
                }
            }
        }
        if (nodes[i].parent2 != "NULL" && !nodes[i].parent2.empty()) {
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (nodes[j].id == nodes[i].parent2) {
                    nodes[j].inDegree++;
                    break;
                }
            }
        }
    }

    // 4. Kahn's Topological Sort (children first, parents after)
    vector<int> readyIndices;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].inDegree == 0) {
            readyIndices.push_back(static_cast<int>(i));
        }
    }

    vector<int> topoIndices;
    while (!readyIndices.empty()) {
        int bestIdx = 0;
        for (size_t k = 1; k < readyIndices.size(); ++k) {
            int idxCurr = readyIndices[k];
            int idxBest = readyIndices[bestIdx];
            if (nodes[idxCurr].id == headCommit) {
                bestIdx = static_cast<int>(k);
            } else if (nodes[idxBest].id != headCommit && nodes[idxCurr].id > nodes[idxBest].id) {
                bestIdx = static_cast<int>(k);
            }
        }

        int currNodeIdx = readyIndices[bestIdx];
        readyIndices.erase(readyIndices.begin() + bestIdx);
        topoIndices.push_back(currNodeIdx);

        string p1 = nodes[currNodeIdx].parent1;
        string p2 = nodes[currNodeIdx].parent2;

        if (p1 != "NULL" && !p1.empty()) {
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (nodes[j].id == p1) {
                    nodes[j].inDegree--;
                    if (nodes[j].inDegree == 0) {
                        readyIndices.push_back(static_cast<int>(j));
                    }
                    break;
                }
            }
        }
        if (p2 != "NULL" && !p2.empty()) {
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (nodes[j].id == p2) {
                    nodes[j].inDegree--;
                    if (nodes[j].inDegree == 0) {
                        readyIndices.push_back(static_cast<int>(j));
                    }
                    break;
                }
            }
        }
    }

    // 5. Draw ASCII Graph
    vector<string> activeCols;

    for (int idx : topoIndices) {
        const NodeMeta& node = nodes[idx];

        int col = -1;
        for (size_t c = 0; c < activeCols.size(); ++c) {
            if (activeCols[c] == node.id) {
                col = static_cast<int>(c);
                break;
            }
        }

        if (col == -1) {
            activeCols.push_back(node.id);
            col = static_cast<int>(activeCols.size() - 1);
        }

        // Commit Node Line
        string nodeLine = "";
        for (size_t c = 0; c < activeCols.size(); ++c) {
            if (static_cast<int>(c) == col) {
                nodeLine += "* ";
            } else {
                nodeLine += "| ";
            }
        }
        nodeLine += "  commit " + node.id;
        if (!node.refs.empty()) {
            nodeLine += " (";
            for (size_t r = 0; r < node.refs.size(); ++r) {
                nodeLine += node.refs[r] + (r + 1 < node.refs.size() ? ", " : "");
            }
            nodeLine += ")";
        }
        cout << nodeLine << endl;

        // Merge details if merge commit
        bool isMerge = (node.parent2 != "NULL" && !node.parent2.empty());
        if (isMerge) {
            string mergeLine = "";
            for (size_t c = 0; c < activeCols.size(); ++c) {
                if (static_cast<int>(c) == col) {
                    mergeLine += "|\\";
                } else {
                    mergeLine += "| ";
                }
            }
            mergeLine += "  Merge: " + node.parent1 + " " + node.parent2;
            cout << mergeLine << endl;
        }

        // Message line
        string msgLine = "";
        for (size_t c = 0; c < activeCols.size(); ++c) {
            msgLine += "| ";
        }
        if (isMerge) {
            msgLine += "| ";
        }
        msgLine += "Message: " + node.message;
        cout << msgLine << endl;

        // Update active columns
        if (isMerge) {
            activeCols[col] = node.parent1;
            activeCols.insert(activeCols.begin() + col + 1, node.parent2);
        } else if (node.parent1 != "NULL" && !node.parent1.empty()) {
            int existingCol = -1;
            for (size_t c = 0; c < activeCols.size(); ++c) {
                if (static_cast<int>(c) != col && activeCols[c] == node.parent1) {
                    existingCol = static_cast<int>(c);
                    break;
                }
            }
            if (existingCol != -1) {
                string joinLine = "";
                for (size_t c = 0; c < activeCols.size(); ++c) {
                    if (static_cast<int>(c) == col) {
                        joinLine += "|/";
                    } else {
                        joinLine += "| ";
                    }
                }
                cout << joinLine << endl;
                activeCols.erase(activeCols.begin() + col);
            } else {
                activeCols[col] = node.parent1;
            }
        } else {
            activeCols.erase(activeCols.begin() + col);
        }
    }
}

// Function to test the custom HashTable implementation (Phase 3 + Dynamic Resizing)
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

    table.insert("beta.txt", "22222");
    table.insert("main.cpp", "12345");
    cout << "Inserted both \"beta.txt\" and \"main.cpp\" into the table." << endl;

    cout << "\nCurrent Bucket Structure:" << endl;
    table.printTable();

    cout << "\nVerifying independent retrieval from the same bucket chain:" << endl;
    cout << "get(\"main.cpp\"): " << table.get("main.cpp") << " (Expected: 12345)" << endl;
    cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << endl;
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: true)" << endl;
    cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << endl;

    cout << "\nTesting getAll() iteration:" << endl;
    auto allItems = table.getAll();
    for (const auto& item : allItems) {
        cout << "  " << item.first << " -> " << item.second << endl;
    }

    cout << "\nRemoving \"main.cpp\" from the collision chain..." << endl;
    table.remove("main.cpp");
    cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: false)" << endl;
    cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << endl;
    cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << endl;

    // 7. Test Dynamic Resizing
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
    cout << "  minigit status" << endl;
    cout << "  minigit hash <filename>" << endl;
    cout << "  minigit add <filename | .>" << endl;
    cout << "  minigit rm <filename> [--cached]" << endl;
    cout << "  minigit commit \"<message>\"" << endl;
    cout << "  minigit branch" << endl;
    cout << "  minigit branch <name>" << endl;
    cout << "  minigit branch -d <name>" << endl;
    cout << "  minigit checkout <branch|commitId> [--force]" << endl;
    cout << "  minigit checkout -b <name> [--force]" << endl;
    cout << "  minigit show <commitId>" << endl;
    cout << "  minigit diff <filename>" << endl;
    cout << "  minigit diff <commitA> <commitB>" << endl;
    cout << "  minigit merge <branch>" << endl;
    cout << "  minigit merge --abort" << endl;
    cout << "  minigit log [--oneline | --graph]" << endl;
    cout << "  minigit config user.name \"<name>\"" << endl;
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
    else if (command == "status") {
        printStatus();
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
            cout << "  minigit add <filename | .>" << endl;
            return 1;
        }
        string target = argv[2];
        if (target == ".") {
            addAll();
        } else {
            addFile(target);
        }
    } else if (command == "rm") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit rm <filename> [--cached]" << endl;
            return 1;
        }
        bool cached = false;
        string targetFile = "";
        for (int i = 2; i < argc; ++i) {
            string arg = argv[i];
            if (arg == "--cached") {
                cached = true;
            } else if (targetFile.empty()) {
                targetFile = arg;
            }
        }
        if (targetFile.empty()) {
            cout << "Usage:" << endl;
            cout << "  minigit rm <filename> [--cached]" << endl;
            return 1;
        }
        removeFile(targetFile, cached);
    } else if (command == "commit") {
        string msg = "";
        if (argc >= 3) {
            msg = argv[2];
        }
        createCommit(msg);
    } else if (command == "show") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit show <commitId>" << endl;
            return 1;
        }
        showCommit(argv[2]);
    } else if (command == "diff") {
        if (argc == 3) {
            diffWorkingFile(argv[2]);
        } else if (argc == 4) {
            diffCommits(argv[2], argv[3]);
        } else {
            cout << "Usage:" << endl;
            cout << "  minigit diff <filename>" << endl;
            cout << "  minigit diff <commitA> <commitB>" << endl;
            return 1;
        }
    } else if (command == "branch") {
        if (argc == 2) {
            listBranches();
        } else if (argc == 3) {
            createBranch(argv[2]);
        } else if (argc == 4 && string(argv[2]) == "-d") {
            deleteBranch(argv[3]);
        } else {
            cout << "Usage:" << endl;
            cout << "  minigit branch" << endl;
            cout << "  minigit branch <name>" << endl;
            cout << "  minigit branch -d <name>" << endl;
            return 1;
        }
    } else if (command == "checkout") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit checkout <branch|commitId> [--force]" << endl;
            cout << "  minigit checkout -b <name> [--force]" << endl;
            return 1;
        }

        bool force = false;
        bool isNewBranch = false;
        string target = "";

        for (int i = 2; i < argc; ++i) {
            string arg = argv[i];
            if (arg == "--force") {
                force = true;
            } else if (arg == "-b") {
                isNewBranch = true;
            } else if (target.empty()) {
                target = arg;
            }
        }

        if (target.empty()) {
            cout << "Usage:" << endl;
            cout << "  minigit checkout <branch|commitId> [--force]" << endl;
            cout << "  minigit checkout -b <name> [--force]" << endl;
            return 1;
        }

        if (isNewBranch) {
            checkoutNewBranch(target, force);
        } else {
            checkoutTarget(target, force);
        }
    } else if (command == "merge") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit merge <branch>" << endl;
            cout << "  minigit merge --abort" << endl;
            return 1;
        }
        string target = argv[2];
        if (target == "--abort") {
            abortMerge();
        } else {
            mergeBranch(target);
        }
    } else if (command == "log") {
        if (argc >= 3) {
            string flag = argv[2];
            if (flag == "--oneline") {
                printCommitLog(true);
            } else if (flag == "--graph") {
                printCommitGraph();
            } else {
                printCommitLog(false);
            }
        } else {
            printCommitLog(false);
        }
    } else if (command == "config") {
        if (argc == 4 && string(argv[2]) == "user.name") {
            setUserConfig(".minigit", "user.name", argv[3]);
            cout << "user.name = " << argv[3] << endl;
        } else if (argc == 3 && string(argv[2]) == "user.name") {
            cout << getUserConfig(".minigit", "user.name", "Unknown") << endl;
        } else {
            cout << "Usage:" << endl;
            cout << "  minigit config user.name \"<name>\"" << endl;
            return 1;
        }
    } else if (command == "test-hashtable") {
        runHashTableTests();
    } else {
        printUsage();
        return 1;
    }

    return 0;
}
