#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
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
    commitOut << "author=" << author << "\n";
    commitOut << "timestamp=" << timestamp << "\n\n";

    for (const auto& entry : stagedEntries) {
        commitOut << entry.first << "|" << entry.second << "\n";
    }
    commitOut.close();

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
            cout << "Message: " << message << endl;
            cout << "Parent:  " << parent << endl;
            cout << "Files (" << files.size() << "): ";
            for (size_t i = 0; i < files.size(); ++i) {
                cout << files[i] << (i + 1 < files.size() ? ", " : "");
            }
            cout << endl;
        }

        currentCommit = parent;
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
    cout << "  minigit log [--oneline]" << endl;
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
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit commit \"<message>\"" << endl;
            return 1;
        }
        createCommit(argv[2]);
    } else if (command == "show") {
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit show <commitId>" << endl;
            return 1;
        }
        showCommit(argv[2]);
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
    } else if (command == "log") {
        bool oneline = false;
        if (argc >= 3 && string(argv[2]) == "--oneline") {
            oneline = true;
        }
        printCommitLog(oneline);
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
