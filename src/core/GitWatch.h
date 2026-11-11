#ifndef GITWATCH_H
#define GITWATCH_H

#include <cstddef>
#include <string>
#include <vector>

// Qt-free helpers for the auto-refresh: where does Git keep its data, which folders must be
// watched to notice a change, and a cheap summary of everything that decides what the graph shows.

namespace Git {

struct GitDirs {
    std::string gitDir;      // the folder with HEAD (a linked worktree has its own)
    std::string commonDir;   // the folder with refs/ and packed-refs (same as gitDir unless a linked worktree)

    [[nodiscard]] bool valid() const { return !gitDir.empty(); }
};

/// Finds the Git folders for `repoPath`, which may be the working folder, any folder inside it,
/// a bare repository, or a .git folder. Understands `.git` files (linked worktrees, submodules).
/// Returns an invalid GitDirs if nothing is found.
[[nodiscard]] GitDirs findGitDirs(const std::string &repoPath);

/// Folders to watch so that every ref update is noticed: the Git folder itself (HEAD and
/// packed-refs are replaced there) and every folder under refs/ (branch, tag and remote updates).
/// Git replaces files instead of editing them, so watching the files themselves would not work.
[[nodiscard]] std::vector<std::string> watchedDirectories(const GitDirs &dirs);

/// A value that changes whenever HEAD, a branch, a tag, a remote branch or packed-refs changes,
/// and stays the same for noise such as index.lock, FETCH_HEAD, *.lock files or the stash.
/// 0 for an invalid GitDirs. Only compare values from the same run of the program.
[[nodiscard]] std::size_t refsFingerprint(const GitDirs &dirs);

} // namespace Git

#endif // GITWATCH_H
