# GitVisualizer

GitVisualizer is a native desktop Git repository client built in *C++23* and *Qt 6, powered by **libgit2*. It combines high-performance custom data structures with a real-time visual commit graph, topological diff viewer, and embedded terminal workflow.

---

## Architecture & Team Contributions

This project is developed collaboratively by a 4-member undergraduate team, with each member owning core systems and custom data structures:

## *Member 1: Classes, Objects & Memory Management & Graphs (DAG)*
- *Core Data Objects (GitTypes.h)*: Defined the structure for CommitNode, Connection paths, table columns, and custom Qt roles.

- *Topological Layout Engine (GraphLayoutEngine.h, GraphLayoutEngine.cpp)*: Created the core algorithmic engine mapping raw commit Directed Acyclic Graphs (DAGs) into (X, Y) lane coordinates, allocating lane colors, and handling branch splits and merges.

- *Graph View & Context Menus (CommitGraphView.cpp)*: Implemented the custom table view wrapper handling right-click context menus for checkout, branching, tagging, resets, and cherry-picking.

### *Member 2: Inheritance, Polymorphism & Virtual Functions & Trees & Stacks*
- *Virtualized Table Models (CommitTableModel.cpp)*: Subclassed QAbstractTableModel with polymorphic virtual overrides (data(), rowCount(), columnCount()) to efficiently render large commit histories.

- *Custom Item Delegates (GraphLaneDelegate.h, GraphLaneDelegate.cpp)*: Implemented polymorphic item delegates using QPainter to render anti-aliased graph lines, Bezier connection splines, nodes with HEAD halos, and branch/tag badges.

- *Custom Stack (CustomContainers.h)*: Implemented a custom node-based linked-list stack data structure with $O(1)$ push/pop complexity and RAII move semantics.

- *Tree Diffs (DiffViewerWidget.cpp)*: Built the tree lookup and diff patch rendering system using libgit2 tree objects, tree-to-tree differences, and syntax-highlighted diff callbacks.

### *Member 3: File Handling, Exception Handling & Streams & UI Architecture*
- *Repository & File Management (GitRepository.h, GitRepository.cpp)*: Implemented safe RAII wrappers for low-level libgit2 handles, repository path. validation, and error/exception checking when opening git repositories on disk.

- *Main Application Architecture (MainWindow.h, MainWindow.cpp)*: Built the primary multi-split window layout, menus, status bars, toolbars, and live QFileSystemWatcher integration to auto-refresh the UI when .git files change.

### *Member 4: Operator Overloading & STL Containers & Queues, Sorting & Searching*
- *Custom Queue (CustomContainers.h)*: Implemented a custom node-based linked-list queue data structure supporting safe FIFO enqueue and dequeue operations.

- *Command Runner (CommandRunner.cpp)*: Built the thread-safe QProcess wrapper that executes CLI commands, preserves environment variables/SSH/GPG states, and sequences operations using the custom queue.

- *Interactive Terminal (TerminalDockWidget.cpp)*: Developed the embedded interactive shell terminal panel with custom text streaming and process execution.


---

## Requirements

- *Language:* Modern C++ (C++23)

- *Framework:* Qt 6 (Widgets, Model/View, Painter, Network)

- *Git Library:* libgit2 (C library bindings)

- *Build System:* CMake (Multi-platform support)

- *Operating System:* Optimized for Linux


---
