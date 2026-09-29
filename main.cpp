#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include "Hash.h"
#include "HashTable.h"

namespace fs = std::filesystem;

// Function to initialize a new MiniGit repository
void initRepository() {
    const std::string repoDir = ".minigit";
    const std::string headFile = repoDir + "/HEAD";

    std::error_code ec;

    // 1. Check if .minigit repository directory already exists
    if (fs::exists(repoDir, ec)) {
        std::cout << "MiniGit repository already exists." << std::endl;
        return;
    }

    // 2. Create the hidden .minigit directory
    if (!fs::create_directory(repoDir, ec)) {
        std::cerr << "Error: Unable to create repository directory '" << repoDir << "'." << std::endl;
        return;
    }

    // 3. Create HEAD file and write the default branch name 'main'
    std::ofstream head(headFile);
    if (!head.is_open()) {
        std::cerr << "Error: Unable to create HEAD file at '" << headFile << "'." << std::endl;
        return;
    }

    head << "main\n";
    head.close();

    std::cout << "Initialized empty MiniGit repository in " << repoDir << "/" << std::endl;
}

// Function to compute and display the hash of a file
void hashFile(const std::string& filename) {
    std::error_code ec;

    // 1. Check if the file exists
    if (!fs::exists(filename, ec)) {
        std::cerr << "Error: File not found." << std::endl;
        return;
    }

    // 2. Check if the path is a directory instead of a regular file
    if (fs::is_directory(filename, ec)) {
        std::cerr << "Error: '" << filename << "' is a directory, not a file." << std::endl;
        return;
    }

    // 3. Open the file in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file '" << filename << "'." << std::endl;
        return;
    }

    // 4. Read the file contents into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // 5. Calculate the hash using our polynomial hash function
    std::string hashValue = calculateHash(content);

    // 6. Display the file name and its computed hash
    std::cout << "File: " << filename << std::endl;
    std::cout << "Hash: " << hashValue << std::endl;
}

// Function to stage a file in MiniGit (Phase 4)
void addFile(const std::string& filename) {
    const std::string repoDir = ".minigit";
    const std::string stagingFile = repoDir + "/staging";

    std::error_code ec;

    // 1. Check if .minigit repository directory exists
    if (!fs::exists(repoDir, ec)) {
        std::cerr << "Error: Not a MiniGit repository." << std::endl;
        std::cerr << "Run 'minigit init' first." << std::endl;
        return;
    }

    // 2. Check if the target file exists
    if (!fs::exists(filename, ec)) {
        std::cerr << "Error: File not found." << std::endl;
        return;
    }

    // 3. Ensure the target path is a regular file, not a directory
    if (fs::is_directory(filename, ec)) {
        std::cerr << "Error: '" << filename << "' is a directory, not a file." << std::endl;
        return;
    }

    // 4. Open and read file content in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file '" << filename << "'." << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // 5. Calculate the file's hash using our polynomial hash function
    std::string hashValue = calculateHash(content);

    // 6. In-memory HashTable to hold staging entries
    HashTable stagingTable;

    // Load existing staging entries from disk into the HashTable (if staging file exists)
    if (fs::exists(stagingFile, ec)) {
        std::ifstream stagingIn(stagingFile);
        if (stagingIn.is_open()) {
            std::string line;
            while (std::getline(stagingIn, line)) {
                if (line.empty()) continue;
                size_t delimiterPos = line.find('|');
                if (delimiterPos != std::string::npos) {
                    std::string fName = line.substr(0, delimiterPos);
                    std::string fHash = line.substr(delimiterPos + 1);
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
    std::ofstream stagingOut(stagingFile);
    if (!stagingOut.is_open()) {
        std::cerr << "Error: Unable to write to staging file at '" << stagingFile << "'." << std::endl;
        return;
    }

    for (const auto& entry : stagingTable.getAll()) {
        stagingOut << entry.first << "|" << entry.second << "\n";
    }
    stagingOut.close();

    // 9. Display confirmation message
    std::cout << "Staged: " << filename << std::endl;
    std::cout << "Hash: " << hashValue << std::endl;
}

// Function to create a new permanent commit snapshot (Phase 5)
void createCommit(const std::string& message) {
    const std::string repoDir = ".minigit";
    const std::string headFile = repoDir + "/HEAD";
    const std::string stagingFile = repoDir + "/staging";
    const std::string commitsDir = repoDir + "/commits";

    std::error_code ec;

    // 1. Verify that .minigit directory exists
    if (!fs::exists(repoDir, ec)) {
        std::cerr << "Error: Not a MiniGit repository." << std::endl;
        std::cerr << "Run 'minigit init' first." << std::endl;
        return;
    }

    // 2. Verify that .minigit/staging exists
    if (!fs::exists(stagingFile, ec)) {
        std::cout << "Error: Nothing to commit." << std::endl;
        return;
    }

    // 3. Read all staged entries into the custom HashTable
    HashTable commitTable;
    std::ifstream stagingIn(stagingFile);
    if (!stagingIn.is_open()) {
        std::cerr << "Error: Unable to open staging file." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(stagingIn, line)) {
        // Strip carriage returns and trailing whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t delimiterPos = line.find('|');
        if (delimiterPos != std::string::npos) {
            std::string fName = line.substr(0, delimiterPos);
            std::string fHash = line.substr(delimiterPos + 1);
            commitTable.insert(fName, fHash);
        }
    }
    stagingIn.close();

    // Check if there is at least one staged entry
    auto stagedEntries = commitTable.getAll();
    if (stagedEntries.empty()) {
        std::cout << "Error: Nothing to commit." << std::endl;
        return;
    }

    // 4. Determine parent commit ID and next commit number from HEAD
    std::string parent = "NULL";
    int commitNumber = 1;

    if (fs::exists(headFile, ec)) {
        std::ifstream headIn(headFile);
        if (headIn.is_open()) {
            std::string headContent;
            std::getline(headIn, headContent);
            headIn.close();

            while (!headContent.empty() && (headContent.back() == '\r' || headContent.back() == ' ')) {
                headContent.pop_back();
            }

            if (!headContent.empty() && headContent != "main" && headContent != "NULL") {
                parent = headContent;
                if (parent.size() > 1 && parent[0] == 'C') {
                    try {
                        commitNumber = std::stoi(parent.substr(1)) + 1;
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
    std::string commitId = buf;

    // 5. Ensure .minigit/commits/ directory exists
    if (!fs::exists(commitsDir, ec)) {
        if (!fs::create_directory(commitsDir, ec)) {
            std::cerr << "Error: Unable to create commits directory." << std::endl;
            return;
        }
    }

    // 6. Write the commit snapshot file (.minigit/commits/<commitId>)
    std::string commitFilePath = commitsDir + "/" + commitId;
    std::ofstream commitOut(commitFilePath);
    if (!commitOut.is_open()) {
        std::cerr << "Error: Unable to create commit file at '" << commitFilePath << "'." << std::endl;
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
    std::ofstream headOut(headFile);
    if (!headOut.is_open()) {
        std::cerr << "Error: Unable to update HEAD reference." << std::endl;
        return;
    }
    headOut << commitId << "\n";
    headOut.close();

    // 8. Print commit confirmation
    std::cout << "[" << commitId << "] " << message << std::endl;
}

// Function to test the custom HashTable implementation
void runHashTableTests() {
    std::cout << "========================================" << std::endl;
    std::cout << "      Phase 3: Custom HashTable Tests   " << std::endl;
    std::cout << "========================================" << std::endl;

    HashTable table;

    // 1. Test Insert
    std::cout << "\n--- [1] Testing Insert ---" << std::endl;
    table.insert("main.cpp", "12345");
    table.insert("README.md", "67890");
    table.insert("test.cpp", "99999");
    std::cout << "Inserted: main.cpp  -> 12345 (Bucket " << table.getBucketIndex("main.cpp") << ")" << std::endl;
    std::cout << "Inserted: README.md -> 67890 (Bucket " << table.getBucketIndex("README.md") << ")" << std::endl;
    std::cout << "Inserted: test.cpp  -> 99999 (Bucket " << table.getBucketIndex("test.cpp") << ")" << std::endl;

    // 2. Test Get
    std::cout << "\n--- [2] Testing Get ---" << std::endl;
    std::cout << "get(\"main.cpp\"):  " << table.get("main.cpp") << " (Expected: 12345)" << std::endl;
    std::cout << "get(\"README.md\"): " << table.get("README.md") << " (Expected: 67890)" << std::endl;
    std::cout << "get(\"test.cpp\"):  " << table.get("test.cpp") << " (Expected: 99999)" << std::endl;

    // 3. Test Contains
    std::cout << "\n--- [3] Testing Contains ---" << std::endl;
    std::cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: true)" << std::endl;
    std::cout << "contains(\"abc.cpp\"):  " << (table.contains("abc.cpp") ? "true" : "false") << " (Expected: false)" << std::endl;

    // 4. Test Update (no duplicate entries)
    std::cout << "\n--- [4] Testing Update ---" << std::endl;
    table.insert("main.cpp", "54321");
    std::cout << "Updated: main.cpp -> 54321" << std::endl;
    std::cout << "get(\"main.cpp\"): " << table.get("main.cpp") << " (Expected: 54321)" << std::endl;

    // 5. Test Remove
    std::cout << "\n--- [5] Testing Remove ---" << std::endl;
    bool removed = table.remove("main.cpp");
    std::cout << "remove(\"main.cpp\"): " << (removed ? "true" : "false") << " (Expected: true)" << std::endl;
    std::cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: false)" << std::endl;
    std::cout << "get(\"main.cpp\"): \"" << table.get("main.cpp") << "\" (Expected: empty string)" << std::endl;

    // 6. Test Collision Handling (Separate Chaining)
    std::cout << "\n--- [6] Testing Collision Handling (Separate Chaining) ---" << std::endl;
    std::cout << "Demonstrating natural collision resolution in Bucket 0:" << std::endl;
    std::cout << "Bucket index for \"main.cpp\": " << table.getBucketIndex("main.cpp") << std::endl;
    std::cout << "Bucket index for \"beta.txt\": " << table.getBucketIndex("beta.txt") << std::endl;

    // Insert both keys into the hash table
    table.insert("beta.txt", "22222");
    table.insert("main.cpp", "12345");
    std::cout << "Inserted both \"beta.txt\" and \"main.cpp\" into the table." << std::endl;

    // Display the internal bucket layout
    std::cout << "\nCurrent Bucket Structure:" << std::endl;
    table.printTable();

    // Verify both items can be independently retrieved despite being in the same bucket
    std::cout << "\nVerifying independent retrieval from the same bucket chain:" << std::endl;
    std::cout << "get(\"main.cpp\"): " << table.get("main.cpp") << " (Expected: 12345)" << std::endl;
    std::cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << std::endl;
    std::cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: true)" << std::endl;
    std::cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << std::endl;

    // Verify getAll()
    std::cout << "\nTesting getAll() iteration:" << std::endl;
    auto allItems = table.getAll();
    for (const auto& item : allItems) {
        std::cout << "  " << item.first << " -> " << item.second << std::endl;
    }

    // Verify removing one colliding key does not affect the other in the chain
    std::cout << "\nRemoving \"main.cpp\" from the collision chain..." << std::endl;
    table.remove("main.cpp");
    std::cout << "contains(\"main.cpp\"): " << (table.contains("main.cpp") ? "true" : "false") << " (Expected: false)" << std::endl;
    std::cout << "contains(\"beta.txt\"): " << (table.contains("beta.txt") ? "true" : "false") << " (Expected: true)" << std::endl;
    std::cout << "get(\"beta.txt\"): " << table.get("beta.txt") << " (Expected: 22222)" << std::endl;

    std::cout << "\n========================================" << std::endl;
    std::cout << "       HashTable Tests Completed!       " << std::endl;
    std::cout << "========================================" << std::endl;
}

// Function to print usage instructions when an unsupported command is entered
void printUsage() {
    std::cout << "Unknown command." << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "  minigit init" << std::endl;
    std::cout << "  minigit hash <filename>" << std::endl;
    std::cout << "  minigit add <filename>" << std::endl;
    std::cout << "  minigit commit \"<message>\"" << std::endl;
    std::cout << "  minigit test-hashtable" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check if at least one argument was passed
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "init") {
        initRepository();
    } else if (command == "hash") {
        // Ensure a filename argument is provided
        if (argc < 3) {
            std::cout << "Usage:" << std::endl;
            std::cout << "  minigit hash <filename>" << std::endl;
            return 1;
        }
        hashFile(argv[2]);
    } else if (command == "add") {
        // Ensure a filename argument is provided
        if (argc < 3) {
            std::cout << "Usage:" << std::endl;
            std::cout << "  minigit add <filename>" << std::endl;
            return 1;
        }
        addFile(argv[2]);
    } else if (command == "commit") {
        // Ensure commit message is provided
        if (argc < 3) {
            std::cout << "Usage:" << std::endl;
            std::cout << "  minigit commit \"<message>\"" << std::endl;
            return 1;
        }
        std::string message = argv[2];
        if (message.empty()) {
            std::cerr << "Error: Commit message cannot be empty." << std::endl;
            return 1;
        }
        createCommit(message);
    } else if (command == "test-hashtable") {
        runHashTableTests();
    } else {
        printUsage();
        return 1;
    }

    return 0;
}
