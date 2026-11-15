#include "core/DiffText.h"

namespace Git {

namespace {

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

// "diff --git a/dir/file b/dir/file"  ->  "dir/file"
std::string pathFromFileHeader(std::string_view line) {
    constexpr std::string_view prefix = "diff --git ";
    const std::string_view rest = line.substr(prefix.size());

    // Without renames both sides name the same file: "a/" + P + " b/" + P. Checking that exact
    // shape keeps names that contain spaces (or " b/") intact.
    if (rest.size() >= 5 && (rest.size() - 5) % 2 == 0) {
        const std::size_t n = (rest.size() - 5) / 2;
        if (rest.substr(0, 2) == "a/" && rest.substr(2 + n, 3) == " b/"
            && rest.substr(2, n) == rest.substr(n + 5)) {
            return std::string(rest.substr(2, n));
        }
    }

    const std::size_t split = rest.rfind(" b/");
    if (split != std::string_view::npos) {
        return std::string(rest.substr(split + 3));
    }
    return std::string(rest);
}

} // namespace

ParsedPatch parsePatch(std::string_view patch) {
    ParsedPatch result;
    bool inHunk = false;
    std::size_t position = 0;

    while (position < patch.size()) {
        const std::size_t end = patch.find('\n', position);
        const bool isLastLine = (end == std::string_view::npos);
        const std::string_view line = patch.substr(position, isLastLine ? std::string_view::npos : end - position);
        position = isLastLine ? patch.size() : end + 1;

        DiffLineKind kind = DiffLineKind::Context;

        if (startsWith(line, "diff --git ")) {
            inHunk = false;
            kind = DiffLineKind::FileHeader;

            DiffFileSection file;
            file.path = pathFromFileHeader(line);
            file.firstLine = result.lineKinds.size();
            result.files.push_back(std::move(file));
        } else if (startsWith(line, "@@")) {
            inHunk = true;
            kind = DiffLineKind::Hunk;
        } else if (!inHunk) {
            // Everything between "diff --git" and the first "@@" describes the file.
            kind = DiffLineKind::FileHeader;
            if (!result.files.empty()) {
                DiffFileSection &file = result.files.back();
                if (startsWith(line, "new file mode")) {
                    file.change = FileChange::Added;
                } else if (startsWith(line, "deleted file mode")) {
                    file.change = FileChange::Deleted;
                } else if (startsWith(line, "Binary files ") || startsWith(line, "GIT binary patch")) {
                    file.binary = true;
                }
            }
        } else if (!line.empty() && line.front() == '+') {
            kind = DiffLineKind::Added;
            if (!result.files.empty()) {
                ++result.files.back().added;
            }
        } else if (!line.empty() && line.front() == '-') {
            kind = DiffLineKind::Removed;
            if (!result.files.empty()) {
                ++result.files.back().removed;
            }
        }
        // else: a context line (" text") or the "\ No newline at end of file" marker.

        result.lineKinds.push_back(kind);
    }

    return result;
}

} // namespace Git
