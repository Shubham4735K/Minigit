#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include "Hash.h"
#include "HashTable.h"

using namespace std;
namespace fs = filesystem;

// Function to initialize a new MiniGit repository
void initRepository() {
    const string repoDir = ".minigit";
    const string headFile = repoDir + "/HEAD";
    const string objectsDir = repoDir + "/objects";

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

    // 3. Create HEAD file and write the default branch name 'main'
    ofstream head(headFile);
    if (!head.is_open()) {
        cerr << "Error: Unable to create HEAD file at '" << headFile << "'." << endl;
        return;
    }

    head << "main\n";
    head.close();

    cout << "Initialized empty MiniGit repository in " << repoDir << "/" << endl;
}

// Function to compute and display the hash of a file
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

    // 3. Open the file in binary mode
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    // 4. Read the file contents into a string
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    // 5. Calculate the hash using our polynomial hash function
    string hashValue = calculateHash(content);

    // 6. Display the file name and its computed hash
    cout << "File: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to stage a file in MiniGit (Phase 4)
void addFile(const string& filename) {
    const string repoDir = ".minigit";
    const string stagingFile = repoDir + "/staging";

    error_code ec;

    // 1. Check if .minigit repository directory exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Check if the target file exists
    if (!fs::exists(filename, ec)) {
        cerr << "Error: File not found." << endl;
        return;
    }

    // 3. Ensure the target path is a regular file, not a directory
    if (fs::is_directory(filename, ec)) {
        cerr << "Error: '" << filename << "' is a directory, not a file." << endl;
        return;
    }

    // 4. Open and read file content in binary mode
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file '" << filename << "'." << endl;
        return;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    // 5. Calculate the file's hash using our polynomial hash function
    string hashValue = calculateHash(content);

    // Save file content as a blob in Content-Addressable Storage (.minigit/objects/<hash>)
    const string objectsDir = repoDir + "/objects";
    if (!fs::exists(objectsDir, ec)) {
        fs::create_directories(objectsDir, ec);
    }
    string blobPath = objectsDir + "/" + hashValue;
    if (!fs::exists(blobPath, ec)) {
        ofstream blobOut(blobPath, ios::binary);
        if (blobOut.is_open()) {
            blobOut << content;
            blobOut.close();
        } else {
            cerr << "Warning: Could not save blob to '" << blobPath << "'." << endl;
        }
    }

    // 6. In-memory HashTable to hold staging entries
    HashTable stagingTable;

    // Load existing staging entries from disk into the HashTable (if staging file exists)
    if (fs::exists(stagingFile, ec)) {
        ifstream stagingIn(stagingFile);
        if (stagingIn.is_open()) {
            string line;
            while (getline(stagingIn, line)) {
                if (line.empty()) continue;
                size_t delimiterPos = line.find('|');
                if (delimiterPos != string::npos) {
                    string fName = line.substr(0, delimiterPos);
                    string fHash = line.substr(delimiterPos + 1);
                    stagingTable.insert(fName, fHash);
                }
            }
            stagingIn.close();
        }
    }

    // 7. Insert or update the staged file in the HashTable
    // (insert automatically updates the value if the key already exists)
    stagingTable.insert(filename, hashValue);

    // 8. Persist the updated HashTable back to .minigit/staging
    ofstream stagingOut(stagingFile);
    if (!stagingOut.is_open()) {
        cerr << "Error: Unable to write to staging file at '" << stagingFile << "'." << endl;
        return;
    }

    for (const auto& entry : stagingTable.getAll()) {
        stagingOut << entry.first << "|" << entry.second << "\n";
    }
    stagingOut.close();

    // 9. Display confirmation message
    cout << "Staged: " << filename << endl;
    cout << "Hash: " << hashValue << endl;
}

// Function to create a new permanent commit snapshot (Phase 5)
void createCommit(const string& message) {
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

    // 2. Verify that .minigit/staging exists
    if (!fs::exists(stagingFile, ec)) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    // 3. Read all staged entries into the custom HashTable
    HashTable commitTable;
    ifstream stagingIn(stagingFile);
    if (!stagingIn.is_open()) {
        cerr << "Error: Unable to open staging file." << endl;
        return;
    }

    string line;
    while (getline(stagingIn, line)) {
        // Strip carriage returns and trailing whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t delimiterPos = line.find('|');
        if (delimiterPos != string::npos) {
            string fName = line.substr(0, delimiterPos);
            string fHash = line.substr(delimiterPos + 1);
            commitTable.insert(fName, fHash);
        }
    }
    stagingIn.close();

    // Check if there is at least one staged entry
    auto stagedEntries = commitTable.getAll();
    if (stagedEntries.empty()) {
        cout << "Error: Nothing to commit." << endl;
        return;
    }

    // 4. Determine parent commit ID and next commit number from HEAD
    string parent = "NULL";
    int commitNumber = 1;

    if (fs::exists(headFile, ec)) {
        ifstream headIn(headFile);
        if (headIn.is_open()) {
            string headContent;
            getline(headIn, headContent);
            headIn.close();

            while (!headContent.empty() && (headContent.back() == '\r' || headContent.back() == ' ')) {
                headContent.pop_back();
            }

            if (!headContent.empty() && headContent != "main" && headContent != "NULL") {
                parent = headContent;
                if (parent.size() > 1 && parent[0] == 'C') {
                    try {
                        commitNumber = stoi(parent.substr(1)) + 1;
                    } catch (...) {
                        commitNumber = 1;
                    }
                }
            }
        }
    }

    // Generate formatted commit ID (C001, C002, ...)
    char buf[16];
    snprintf(buf, sizeof(buf), "C%03d", commitNumber);
    string commitId = buf;

    // 5. Ensure .minigit/commits/ directory exists
    if (!fs::exists(commitsDir, ec)) {
        if (!fs::create_directory(commitsDir, ec)) {
            cerr << "Error: Unable to create commits directory." << endl;
            return;
        }
    }

    // 6. Write the commit snapshot file (.minigit/commits/<commitId>)
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

    // 7. Update HEAD to point to this new latest commit
    ofstream headOut(headFile);
    if (!headOut.is_open()) {
        cerr << "Error: Unable to update HEAD reference." << endl;
        return;
    }
    headOut << commitId << "\n";
    headOut.close();

    // 8. Print commit confirmation
    cout << "[" << commitId << "] " << message << endl;
}

// Function to checkout a past commit and restore files (Phase 7)
void checkoutCommit(const string& commitId) {
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

    // 2. Check if the specified commit file exists
    if (!fs::exists(commitFilePath, ec)) {
        cerr << "Error: Commit '" << commitId << "' does not exist." << endl;
        return;
    }

    // 3. Read commit file and parse snapshot files
    ifstream commitIn(commitFilePath);
    if (!commitIn.is_open()) {
        cerr << "Error: Unable to open commit file at '" << commitFilePath << "'." << endl;
        return;
    }

    string line;
    vector<pair<string, string>> filesToRestore;
    string commitMessage = "";

    while (getline(commitIn, line)) {
        // Strip trailing carriage returns and whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        if (line.rfind("id=", 0) == 0 || line.rfind("parent=", 0) == 0) {
            continue;
        }
        if (line.rfind("message=", 0) == 0) {
            commitMessage = line.substr(8);
            continue;
        }

        // Parse filename|hash
        size_t delimiterPos = line.find('|');
        if (delimiterPos != string::npos) {
            string fName = line.substr(0, delimiterPos);
            string fHash = line.substr(delimiterPos + 1);
            filesToRestore.push_back({fName, fHash});
        }
    }
    commitIn.close();

    // 4. Restore files from .minigit/objects/<hash> into the working directory
    int restoredCount = 0;
    HashTable restoredStagingTable;

    for (const auto& entry : filesToRestore) {
        const string& fName = entry.first;
        const string& fHash = entry.second;
        string blobPath = objectsDir + "/" + fHash;

        if (!fs::exists(blobPath, ec)) {
            cerr << "Warning: Blob for '" << fName << "' (" << fHash << ") not found in objects store. Skipping." << endl;
            continue;
        }

        ifstream blobIn(blobPath, ios::binary);
        if (!blobIn.is_open()) {
            cerr << "Warning: Unable to read blob for '" << fName << "'." << endl;
            continue;
        }

        stringstream buffer;
        buffer << blobIn.rdbuf();
        string content = buffer.str();
        blobIn.close();

        // Ensure parent directories exist if fName is a relative path
        fs::path filePath(fName);
        if (filePath.has_parent_path()) {
            fs::create_directories(filePath.parent_path(), ec);
        }

        ofstream fileOut(fName, ios::binary);
        if (!fileOut.is_open()) {
            cerr << "Warning: Unable to restore file '" << fName << "'." << endl;
            continue;
        }
        fileOut << content;
        fileOut.close();

        restoredStagingTable.insert(fName, fHash);
        restoredCount++;
    }

    // 5. Synchronize .minigit/staging with the checked-out commit
    ofstream stagingOut(stagingFile);
    if (stagingOut.is_open()) {
        for (const auto& entry : restoredStagingTable.getAll()) {
            stagingOut << entry.first << "|" << entry.second << "\n";
        }
        stagingOut.close();
    }

    // 6. Update HEAD pointer to this commit
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
    const string headFile = repoDir + "/HEAD";
    const string commitsDir = repoDir + "/commits";

    error_code ec;

    // 1. Verify .minigit repository exists
    if (!fs::exists(repoDir, ec)) {
        cerr << "Error: Not a MiniGit repository." << endl;
        cerr << "Run 'minigit init' first." << endl;
        return;
    }

    // 2. Read HEAD file to find starting commit node
    if (!fs::exists(headFile, ec)) {
        cout << "No commits yet." << endl;
        return;
    }

    ifstream headIn(headFile);
    if (!headIn.is_open()) {
        cerr << "Error: Unable to read HEAD file." << endl;
        return;
    }

    string currentCommit;
    getline(headIn, currentCommit);
    headIn.close();

    // Strip carriage returns and trailing whitespace
    while (!currentCommit.empty() && (currentCommit.back() == '\r' || currentCommit.back() == ' ')) {
        currentCommit.pop_back();
    }

    // If HEAD is at the default branch or empty, no commit has been made
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
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
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

// Function to test the custom HashTable implementation
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

    // Insert both keys into the hash table
    table.insert("beta.txt", "22222");
    table.insert("main.cpp", "12345");
    cout << "Inserted both \"beta.txt\" and \"main.cpp\" into the table." << endl;

    // Display the internal bucket layout
    cout << "\nCurrent Bucket Structure:" << endl;
    table.printTable();

    // Verify both items can be independently retrieved despite being in the same bucket
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
    cout << "  minigit checkout <commitId>" << endl;
    cout << "  minigit log" << endl;
    cout << "  minigit test-hashtable" << endl;
}

int main(int argc, char* argv[]) {
    // Check if at least one argument was passed
    if (argc < 2) {
        printUsage();
        return 1;
    }

    string command = argv[1];

    if (command == "init") {
        initRepository();
    } 
    else if (command == "hash") {
        // Ensure a filename argument is provided
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit hash <filename>" << endl;
            return 1;
        }
        hashFile(argv[2]);
    } else if (command == "add") {
        // Ensure a filename argument is provided
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit add <filename>" << endl;
            return 1;
        }
        addFile(argv[2]);
    } else if (command == "commit") {
        // Ensure commit message is provided
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit commit \"<message>\"" << endl;
            return 1;
        }
        string message = argv[2];
        if (message.empty()) {
            cerr << "Error: Commit message cannot be empty." << endl;
            return 1;
        }
        createCommit(message);
    } else if (command == "checkout") {
        // Ensure commitId argument is provided
        if (argc < 3) {
            cout << "Usage:" << endl;
            cout << "  minigit checkout <commitId>" << endl;
            return 1;
        }
        checkoutCommit(argv[2]);
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
