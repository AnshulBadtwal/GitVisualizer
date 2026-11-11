#include "core/GitWatch.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string_view>

namespace Git {

namespace fs = std::filesystem;

namespace {

// First 4 KB of a file. Ref files hold one line, so this is all of them.
std::string readSmallFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::string text;
    if (!in) {
        return text;
    }
    text.resize(4096);
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(in.gcount()));
    return text;
}

std::string firstLine(const fs::path &path) {
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
    return line;
}

// Normalised text without a trailing separator, so equal folders compare equal.
std::string cleaned(const fs::path &path) {
    fs::path normal = path.lexically_normal();
    if (!normal.has_filename()) {
        normal = normal.parent_path();
    }
    return normal.string();
}

bool looksLikeGitDir(const fs::path &dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "HEAD", ec) && fs::is_directory(dir / "refs", ec);
}

// Linked worktrees keep refs in the main repository and name it in a "commondir" file.
fs::path commonDirOf(const fs::path &gitDir) {
    const fs::path pointer = gitDir / "commondir";
    std::error_code ec;
    if (fs::is_regular_file(pointer, ec)) {
        const std::string target = firstLine(pointer);
        if (!target.empty()) {
            fs::path common(target);
            if (common.is_relative()) {
                common = gitDir / common;
            }
            return common;
        }
    }
    return gitDir;
}

GitDirs makeDirs(const fs::path &gitDir) {
    GitDirs dirs;
    dirs.gitDir = cleaned(gitDir);
    dirs.commonDir = cleaned(commonDirOf(gitDir));
    return dirs;
}

bool startsWith(const std::string &text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

GitDirs findGitDirs(const std::string &repoPath) {
    if (repoPath.empty()) {
        return {};
    }

    std::error_code ec;
    fs::path dir = fs::absolute(fs::path(repoPath), ec);
    if (ec) {
        return {};
    }
    dir = dir.lexically_normal();
    if (!dir.has_filename()) {
        dir = dir.parent_path();
    }

    // A bare repository, or the .git folder itself.
    if (looksLikeGitDir(dir)) {
        return makeDirs(dir);
    }

    // Otherwise look for ".git" here and in the parent folders, like Git does.
    for (;;) {
        const fs::path dotGit = dir / ".git";

        if (fs::is_directory(dotGit, ec)) {
            return makeDirs(dotGit);
        }

        if (fs::is_regular_file(dotGit, ec)) {
            // Linked worktrees and submodules: ".git" is a file saying "gitdir: <path>".
            const std::string line = firstLine(dotGit);
            constexpr std::string_view prefix = "gitdir:";
            if (startsWith(line, prefix)) {
                std::string target = line.substr(prefix.size());
                target.erase(0, target.find_first_not_of(" \t"));
                if (!target.empty()) {
                    fs::path gitDir(target);
                    if (gitDir.is_relative()) {
                        gitDir = dir / gitDir;
                    }
                    return makeDirs(gitDir);
                }
            }
        }

        const fs::path parent = dir.parent_path();
        if (parent.empty() || parent == dir) {
            break;
        }
        dir = parent;
    }

    return {};
}

std::vector<std::string> watchedDirectories(const GitDirs &dirs) {
    std::vector<std::string> result;
    if (!dirs.valid()) {
        return result;
    }

    result.push_back(dirs.gitDir);
    if (dirs.commonDir != dirs.gitDir) {
        result.push_back(dirs.commonDir);
    }

    const fs::path refs = fs::path(dirs.commonDir) / "refs";
    std::error_code ec;
    if (fs::is_directory(refs, ec)) {
        result.push_back(cleaned(refs));

        fs::recursive_directory_iterator it(refs, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code entryEc;
            if (it->is_directory(entryEc)) {
                result.push_back(cleaned(it->path()));
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::size_t refsFingerprint(const GitDirs &dirs) {
    if (!dirs.valid()) {
        return 0;
    }

    const fs::path gitDir(dirs.gitDir);
    const fs::path commonDir(dirs.commonDir);
    std::error_code ec;

    std::vector<std::string> parts;
    parts.push_back("HEAD\n" + readSmallFile(gitDir / "HEAD"));
    parts.push_back("shallow\n" + readSmallFile(commonDir / "shallow"));

    // packed-refs can be large: size and modification time are enough, Git replaces the whole file.
    const fs::path packed = commonDir / "packed-refs";
    if (fs::is_regular_file(packed, ec)) {
        std::error_code sizeEc;
        std::error_code timeEc;
        const auto size = fs::file_size(packed, sizeEc);
        const auto time = fs::last_write_time(packed, timeEc).time_since_epoch().count();
        parts.push_back("packed-refs\n" + std::to_string(size) + ":" + std::to_string(time));
    }

    // Loose refs, but only the kinds the graph shows (not the stash or notes).
    const fs::path refs = commonDir / "refs";
    if (fs::is_directory(refs, ec)) {
        fs::recursive_directory_iterator it(refs, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code entryEc;
            if (!it->is_regular_file(entryEc)) {
                continue;
            }
            const fs::path &file = it->path();
            if (file.extension() == ".lock") {
                continue;   // exists only for a moment while Git updates a ref
            }
            const std::string name = file.lexically_relative(commonDir).generic_string();
            if (!startsWith(name, "refs/heads/") && !startsWith(name, "refs/remotes/")
                && !startsWith(name, "refs/tags/")) {
                continue;
            }
            parts.push_back(name + "\n" + readSmallFile(file));
        }
    }

    // Directory order is not defined, so sort before combining.
    std::sort(parts.begin(), parts.end());
    const std::hash<std::string> hasher;
    auto hash = static_cast<std::size_t>(1469598103934665603ULL);
    for (const std::string &part : parts) {
        hash = (hash ^ hasher(part)) * static_cast<std::size_t>(1099511628211ULL);
    }
    return hash;
}

} // namespace Git
