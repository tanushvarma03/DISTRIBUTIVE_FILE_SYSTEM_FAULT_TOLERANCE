# Distributed File System (DFS)

A simple C++ implementation of a distributed file system with automatic replication, node failure simulation, and metadata persistence.

## Features

- **File Replication**: Automatically replicates uploaded files across 3 active nodes
- **Fault Tolerance**: Simulate node failures and recoveries with automatic health checks
- **Metadata Persistence**: Stores file-to-node mappings on disk for recovery after restarts
- **Auto Re-replication**: Automatically restores replication factor when nodes fail/recover
- **Error Handling**: Comprehensive try-catch blocks for filesystem operations
- **Input Validation**: Validates node IDs and command arguments
- **Space-Separated Filenames**: Supports filenames with spaces via improved CLI parsing

## Building

Requires **C++17** or later.

```bash
g++ -std=c++17 DFS.cpp -o dfs
```

## Running

```bash
./dfs
```

This creates 4 local nodes (`node_1/`, `node_2/`, `node_3/`, `node_4/`) and a metadata file (`metadata.txt`).

## Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `upload` | `upload <filename>` | Upload and replicate file to 3 active nodes |
| `download` | `download <filename>` | Download file from any active replica |
| `delete` | `delete <filename>` | Delete file from all nodes |
| `list` | `list` | Show all stored files and their replicas |
| `fail` | `fail <node_id>` | Simulate node failure (1-4) |
| `recover` | `recover <node_id>` | Recover a failed node |
| `nodes` | `nodes` | Show all nodes and their status |
| `exit` | `exit` | Quit the program |

## Example Session

```
DFS> upload mydata.txt
[UPLOAD SUCCESS] File replicated to nodes: 1 2 3 

DFS> list
FILES IN DFS:
 - mydata.txt → Nodes: 1 2 3 

DFS> fail 1
[NODE FAILED] Node 1 is inactive.

DFS> download mydata.txt
[DOWNLOAD SUCCESS] File downloaded from Node 2

DFS> recover 1
[NODE RECOVERED] Node 1 is active.
RE-REPLICATED: File 'mydata.txt' restored to Node 1.

DFS> delete mydata.txt
[DELETE SUCCESS] File removed from DFS.

DFS> exit
```

## Implementation Details

### Architecture

- **Node Class**: Represents a storage node with an active/failed status and local directory
- **DistributedFS Class**: Manages nodes, file replication, and metadata operations
- **Metadata Storage**: Text-based file (`metadata.txt`) with format: `filename:node_id1,node_id2,...`

### Key Features

1. **Replication**: Files are copied to the first 3 active nodes sequentially
2. **Fault Tolerance**: Downloads from any active replica; warns if replicas < 2
3. **Auto-Healing**: Re-replication automatically restores copies when nodes recover
4. **Persistence**: Metadata survives program restarts via `metadata.txt`

### Error Handling

- Filesystem operations wrapped in try-catch blocks
- Node ID validation prevents out-of-bounds access
- Graceful handling of missing files and inactive replicas
- Clear error messages for user feedback

## Limitations

- **No Concurrency**: Not thread-safe; single-threaded CLI
- **Simple Placement**: Uses sequential node selection (no hashing)
- **Text-Based Metadata**: Simple format; does not support complex queries
- **No Versioning**: Reuploading same filename overwrites old metadata
- **Local Storage Only**: All nodes are local directories; no network support

## Future Enhancements

- Add JSON-based metadata for better structure
- Implement distributed consensus (Raft/Paxos) for multi-node coordination
- Support network replication across multiple machines
- Add file versioning and rollback support
- Implement deterministic placement via consistent hashing
- Add concurrency control (mutexes) for thread-safe operations
- Support for large files with chunking
- Add backup and recovery commands

## Author

Abhinav — CSE316 Project

## License

Open source for educational purposes.
