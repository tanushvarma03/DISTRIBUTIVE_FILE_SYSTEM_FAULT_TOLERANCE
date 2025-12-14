#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

class Node {
public:
    int id;
    bool active;
    fs::path directory;

    Node(int id) {
        this->id = id;
        this->active = true;
        this->directory = "node_" + to_string(id);

        if (!fs::exists(directory))
            fs::create_directory(directory);
    }

    void fail() { active = false; }
    void recover() { active = true; }
};

class DistributedFS {
private:
    vector<Node> nodes;

    // metadata: filename → node IDs storing it
    map<string, vector<int>> metadata;

    const int REPLICATION = 3;
    const string METADATA_FILE = "metadata.txt";

    // Save metadata to file
    void saveMetadata() {
        try {
            ofstream file(METADATA_FILE);
            for (auto &entry : metadata) {
                file << entry.first << ":";
                for (int id : entry.second) {
                    file << id << ",";
                }
                file << "\n";
            }
            file.close();
        } catch (const exception &e) {
            cout << "Warning: Failed to save metadata: " << e.what() << "\n";
        }
    }

    // Load metadata from file
    void loadMetadata() {
        try {
            if (!fs::exists(METADATA_FILE)) return;

            ifstream file(METADATA_FILE);
            string line;
            while (getline(file, line)) {
                if (line.empty()) continue;

                size_t colonPos = line.find(':');
                if (colonPos == string::npos) continue;

                string filename = line.substr(0, colonPos);
                string nodeStr = line.substr(colonPos + 1);

                vector<int> nodeList;
                stringstream ss(nodeStr);
                string token;
                while (getline(ss, token, ',')) {
                    if (!token.empty()) {
                        nodeList.push_back(stoi(token));
                    }
                }

                if (!nodeList.empty()) {
                    metadata[filename] = nodeList;
                }
            }
            file.close();
            cout << "[SYSTEM] Metadata loaded from disk.\n\n";
        } catch (const exception &e) {
            cout << "Warning: Failed to load metadata: " << e.what() << "\n";
        }
    }

public:
    DistributedFS(int totalNodes) {
        for (int i = 1; i <= totalNodes; i++)
            nodes.emplace_back(i);

        cout << "[DFS] Initialized with " << totalNodes << " nodes.\n";
        loadMetadata();
    }

    // Upload file + replicate to 3 nodes
    void upload(string filename) {
        if (!fs::exists(filename)) {
            cout << "Error: File not found.\n";
            return;
        }

        vector<int> usedNodes;
        int replicated = 0;

        try {
            for (auto &node : nodes) {
                if (node.active) {
                    fs::copy(filename, node.directory / filename,
                             fs::copy_options::overwrite_existing);

                    usedNodes.push_back(node.id);
                    replicated++;
                }
                if (replicated == REPLICATION)
                    break;
            }
        } catch (const fs::filesystem_error &e) {
            cout << "Error during file replication: " << e.what() << "\n";
            return;
        }

        if (replicated < REPLICATION) {
            cout << "Error: Not enough active nodes for 3 replicas!\n";
            return;
        }

        metadata[filename] = usedNodes;

        cout << "[UPLOAD SUCCESS] File replicated to nodes: ";
        for (int id : usedNodes) cout << id << " ";
        cout << "\n\n";
        
        saveMetadata();
    }

    // Download from any active node
    void download(string filename) {
        if (!metadata.count(filename)) {
            cout << "Error: File not found in DFS.\n";
            return;
        }

        try {
            for (int nodeID : metadata[filename]) {
                Node &node = nodes[nodeID - 1];

                if (node.active) {
                    fs::copy(node.directory / filename,
                             "downloaded_" + filename,
                             fs::copy_options::overwrite_existing);

                    cout << "[DOWNLOAD SUCCESS] File downloaded from Node "
                         << nodeID << "\n";
                    return;
                }
            }
        } catch (const fs::filesystem_error &e) {
            cout << "Error during download: " << e.what() << "\n";
            return;
        }

        cout << "[ERROR] All replicas are unavailable. File cannot be downloaded.\n";
    }

    // Delete file from all nodes
    void deleteFile(string filename) {
        if (!metadata.count(filename)) {
            cout << "Error: File not found.\n";
            return;
        }

        try {
            for (int nodeID : metadata[filename]) {
                fs::remove(nodes[nodeID - 1].directory / filename);
            }
        } catch (const fs::filesystem_error &e) {
            cout << "Error during deletion: " << e.what() << "\n";
            return;
        }

        metadata.erase(filename);

        cout << "[DELETE SUCCESS] File removed from DFS.\n\n";
        
        saveMetadata();
    }

    // List files with replicas
    void listFiles() {
        if (metadata.empty()) {
            cout << "(Empty) No files stored.\n\n";
            return;
        }

        cout << "\nFILES IN DFS:\n";
        for (auto &entry : metadata) {
            cout << " - " << entry.first << " → Nodes: ";
            for (int nodeID : entry.second) cout << nodeID << " ";
            cout << "\n";
        }
        cout << endl;
    }

    // Fail a node + check warnings
    void failNode(int id) {
        if (id < 1 || id > (int)nodes.size()) {
            cout << "Error: Invalid node ID " << id << ".\n";
            return;
        }
        nodes[id - 1].fail();
        cout << "[NODE FAILED] Node " << id << " is inactive.\n";

        checkReplicaHealth();
        cout << "\n";
    }

    // Recover a node
    void recoverNode(int id) {
        if (id < 1 || id > (int)nodes.size()) {
            cout << "Error: Invalid node ID " << id << ".\n";
            return;
        }
        nodes[id - 1].recover();
        cout << "[NODE RECOVERED] Node " << id << " is active.\n";

        checkReplicaHealth();
        cout << "\n";
    }

    // Show all nodes and their status
    void showNodes() {
        cout << "\nNODE STATUS:\n";
        for (auto &node : nodes) {
            cout << "Node " << node.id << ": "
                 << (node.active ? "Active" : "Failed") << "\n";
        }
        cout << endl;
    }

    // NEW FEATURE: Automatic warnings if replicas < 2
    void checkReplicaHealth() {
        for (auto &entry : metadata) {
            string file = entry.first;
            vector<int> &nodeList = entry.second;

            int activeCount = 0;
            for (int id : nodeList) {
                if (nodes[id - 1].active)
                    activeCount++;
            }

            if (activeCount < 2) {
                cout << "WARNING: File '" << file
                     << "' has only " << activeCount
                     << " active replicas! Data loss risk!\n";
                
                // Attempt to re-replicate
                reReplicateFile(file);
            }
        }
    }

    // Re-replicate file to restore replication factor
    void reReplicateFile(string filename) {
        if (!metadata.count(filename)) return;

        vector<int> &currentNodes = metadata[filename];
        int activeReplicas = 0;

        // Count active replicas
        for (int id : currentNodes) {
            if (nodes[id - 1].active)
                activeReplicas++;
        }

        if (activeReplicas >= REPLICATION) return; // Already replicated enough

        // Find the first active replica to copy from
        int sourceNodeId = -1;
        for (int id : currentNodes) {
            if (nodes[id - 1].active) {
                sourceNodeId = id;
                break;
            }
        }

        if (sourceNodeId == -1) return; // No active replica to copy from

        try {
            // Find inactive nodes in current list and try to restore on them
            for (int id : currentNodes) {
                if (!nodes[id - 1].active && activeReplicas < REPLICATION) {
                    fs::copy(nodes[sourceNodeId - 1].directory / filename,
                             nodes[id - 1].directory / filename,
                             fs::copy_options::overwrite_existing);
                    activeReplicas++;
                    cout << "RE-REPLICATED: File '" << filename << "' restored to Node " << id << ".\n";
                }
            }

            // If still below REPLICATION factor, add to new active nodes
            if (activeReplicas < REPLICATION) {
                for (auto &node : nodes) {
                    if (node.active && find(currentNodes.begin(), currentNodes.end(), node.id) == currentNodes.end()) {
                        fs::copy(nodes[sourceNodeId - 1].directory / filename,
                                 node.directory / filename,
                                 fs::copy_options::overwrite_existing);
                        currentNodes.push_back(node.id);
                        activeReplicas++;
                        cout << "RE-REPLICATED: File '" << filename << "' added to Node " << node.id << ".\n";
                        if (activeReplicas >= REPLICATION) break;
                    }
                }
            }

            saveMetadata();
        } catch (const fs::filesystem_error &e) {
            cout << "Error during re-replication: " << e.what() << "\n";
        }
    }
};

int main() {
    DistributedFS dfs(4); // 4 nodes recommended for triple replication

    string line, cmd, arg;

    cout << "\n=== DISTRIBUTED FILE SYSTEM ===\n";
    cout << "Commands: upload <file>, download <file>, delete <file>, list, fail <id>, recover <id>, nodes, exit\n\n";

    while (true) {
        cout << "DFS> ";
        getline(cin, line);

        if (line.empty()) continue;

        stringstream ss(line);
        ss >> cmd;

        if (cmd == "upload") {
            getline(ss, arg);
            // Trim leading whitespace from arg
            arg.erase(0, arg.find_first_not_of(" \t"));
            if (!arg.empty()) dfs.upload(arg);
            else cout << "Usage: upload <filename>\n";
        }
        else if (cmd == "download") {
            getline(ss, arg);
            arg.erase(0, arg.find_first_not_of(" \t"));
            if (!arg.empty()) dfs.download(arg);
            else cout << "Usage: download <filename>\n";
        }
        else if (cmd == "delete") {
            getline(ss, arg);
            arg.erase(0, arg.find_first_not_of(" \t"));
            if (!arg.empty()) dfs.deleteFile(arg);
            else cout << "Usage: delete <filename>\n";
        }
        else if (cmd == "list") {
            dfs.listFiles();
        }
        else if (cmd == "fail") {
            ss >> arg;
            if (!arg.empty()) {
                int nodeId = stoi(arg);
                dfs.failNode(nodeId);
            }
            else cout << "Usage: fail <node_id>\n";
        }
        else if (cmd == "recover") {
            ss >> arg;
            if (!arg.empty()) {
                int nodeId = stoi(arg);
                dfs.recoverNode(nodeId);
            }
            else cout << "Usage: recover <node_id>\n";
        }
        else if (cmd == "nodes") {
            dfs.showNodes();
        }
        else if (cmd == "exit") {
            break;
        }
        else {
            cout << "Invalid command. Type 'help' for usage.\n";
        }
    }

    return 0;
}
