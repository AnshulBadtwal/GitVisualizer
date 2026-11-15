#ifndef DIFFTEXT_H
#define DIFFTEXT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Qt-free reading of unified-diff text (what `git diff` prints). The diff viewer colors lines
// with it and the details panel builds its list of changed files from it, so both agree on
// what a line is. Pure functions: testable without a repository or a QApplication.

namespace Git {

enum class DiffLineKind : unsigned char {
    FileHeader,   // "diff --git", "index", "---", "+++", "new file mode", "Binary files ..."
    Hunk,         // "@@ -1,3 +1,4 @@ ..."
    Added,        // "+text"
    Removed,      // "-text"
    Context       // " text" and "\ No newline at end of file"
};

enum class FileChange : unsigned char { Modified, Added, Deleted };

// One file of the patch.
struct DiffFileSection {
    std::string path;
    FileChange change{FileChange::Modified};
    bool binary{false};
    std::size_t firstLine{0};   // line index of its "diff --git" line
    int added{0};               // number of "+" lines
    int removed{0};             // number of "-" lines
};

struct ParsedPatch {
    std::vector<DiffLineKind> lineKinds;   // one entry per line, in order
    std::vector<DiffFileSection> files;
};

/// Splits `patch` into lines at '\n' (a final '\n' does not start another line) and classifies
/// each one. Whether a line starting with "---" is a file header or a removed line depends on
/// whether a hunk ("@@") has started, so a removed SQL comment is still counted as removed.
[[nodiscard]] ParsedPatch parsePatch(std::string_view patch);

} // namespace Git

#endif // DIFFTEXT_H
