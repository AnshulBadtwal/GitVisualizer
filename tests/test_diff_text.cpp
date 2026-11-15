#include <gtest/gtest.h>
#include "core/DiffText.h"

using namespace Git;

namespace {

// Line numbers: 0 diff, 1 index, 2 ---, 3 +++, 4 @@, 5 keep, 6 -old, 7 +new, 8 keep,
//               9 diff, 10 new file mode, 11 index, 12 ---, 13 +++, 14 @@, 15 +one, 16 +two
const char *const kTwoFiles =
    "diff --git a/src/a.cpp b/src/a.cpp\n"
    "index 111..222 100644\n"
    "--- a/src/a.cpp\n"
    "+++ b/src/a.cpp\n"
    "@@ -1,3 +1,3 @@\n"
    " keep\n"
    "-old\n"
    "+new\n"
    " keep\n"
    "diff --git a/b.txt b/b.txt\n"
    "new file mode 100644\n"
    "index 000..333\n"
    "--- /dev/null\n"
    "+++ b/b.txt\n"
    "@@ -0,0 +1,2 @@\n"
    "+one\n"
    "+two\n";

} // namespace

TEST(DiffText, EmptyPatchHasNoLinesAndNoFiles) {
    const ParsedPatch parsed = parsePatch("");
    EXPECT_EQ(parsed.lineKinds.size(), 0u);
    EXPECT_EQ(parsed.files.size(), 0u);
}

TEST(DiffText, SplitsFilesAndCountsLines) {
    const ParsedPatch parsed = parsePatch(kTwoFiles);
    ASSERT_NE(parsed.files.size(), 0u);
    EXPECT_EQ(parsed.lineKinds.size(), 17u);   // the final '\n' does not start an 18th line
    EXPECT_EQ(parsed.files.size(), 2u);

    EXPECT_EQ(parsed.files[0].path, std::string("src/a.cpp"));
    EXPECT_EQ(parsed.files[0].firstLine, 0u);
    EXPECT_EQ(parsed.files[0].added, 1);
    EXPECT_EQ(parsed.files[0].removed, 1);
    EXPECT_TRUE(parsed.files[0].change == FileChange::Modified);

    EXPECT_EQ(parsed.files[1].path, std::string("b.txt"));
    EXPECT_EQ(parsed.files[1].firstLine, 9u);
    EXPECT_EQ(parsed.files[1].added, 2);
    EXPECT_EQ(parsed.files[1].removed, 0);
    EXPECT_TRUE(parsed.files[1].change == FileChange::Added);
}

TEST(DiffText, ClassifiesEveryLine) {
    const ParsedPatch parsed = parsePatch(kTwoFiles);
    ASSERT_EQ(parsed.lineKinds.size(), 17u);
    EXPECT_TRUE(parsed.lineKinds[0] == DiffLineKind::FileHeader);
    EXPECT_TRUE(parsed.lineKinds[2] == DiffLineKind::FileHeader);   // "--- a/src/a.cpp"
    EXPECT_TRUE(parsed.lineKinds[3] == DiffLineKind::FileHeader);   // "+++ b/src/a.cpp"
    EXPECT_TRUE(parsed.lineKinds[4] == DiffLineKind::Hunk);
    EXPECT_TRUE(parsed.lineKinds[5] == DiffLineKind::Context);
    EXPECT_TRUE(parsed.lineKinds[6] == DiffLineKind::Removed);
    EXPECT_TRUE(parsed.lineKinds[7] == DiffLineKind::Added);
    EXPECT_TRUE(parsed.lineKinds[9] == DiffLineKind::FileHeader);   // second "diff --git"
    EXPECT_TRUE(parsed.lineKinds[12] == DiffLineKind::FileHeader);  // "--- /dev/null"
    EXPECT_TRUE(parsed.lineKinds[15] == DiffLineKind::Added);
}

TEST(DiffText, RemovedLineThatLooksLikeAFileHeaderIsStillRemoved) {
    // The file had the SQL comment "-- comment"; removing it prints "--- comment".
    const ParsedPatch parsed = parsePatch(
        "diff --git a/q.sql b/q.sql\n"
        "index 1..2 100644\n"
        "--- a/q.sql\n"
        "+++ b/q.sql\n"
        "@@ -1,2 +1,1 @@\n"
        "--- comment\n"
        " select 1;\n");
    ASSERT_EQ(parsed.lineKinds.size(), 7u);
    EXPECT_TRUE(parsed.lineKinds[2] == DiffLineKind::FileHeader);
    EXPECT_TRUE(parsed.lineKinds[5] == DiffLineKind::Removed);
    EXPECT_TRUE(parsed.lineKinds[6] == DiffLineKind::Context);
    EXPECT_EQ(parsed.files[0].removed, 1);
    EXPECT_EQ(parsed.files[0].added, 0);
}

TEST(DiffText, DeletedFile) {
    const ParsedPatch parsed = parsePatch(
        "diff --git a/old.txt b/old.txt\n"
        "deleted file mode 100644\n"
        "index 444..000\n"
        "--- a/old.txt\n"
        "+++ /dev/null\n"
        "@@ -1 +0,0 @@\n"
        "-gone\n");
    ASSERT_EQ(parsed.files.size(), 1u);
    EXPECT_TRUE(parsed.files[0].change == FileChange::Deleted);
    EXPECT_EQ(parsed.files[0].removed, 1);
}

TEST(DiffText, BinaryFileHasNoLineCounts) {
    const ParsedPatch parsed = parsePatch(
        "diff --git a/img.png b/img.png\n"
        "index 555..666 100644\n"
        "Binary files a/img.png and b/img.png differ\n");
    ASSERT_EQ(parsed.files.size(), 1u);
    EXPECT_TRUE(parsed.files[0].binary);
    EXPECT_EQ(parsed.files[0].added, 0);
    EXPECT_EQ(parsed.files[0].removed, 0);
    EXPECT_TRUE(parsed.lineKinds[2] == DiffLineKind::FileHeader);
}

TEST(DiffText, NoNewlineMarkerIsContext) {
    const ParsedPatch parsed = parsePatch(
        "diff --git a/n.txt b/n.txt\n"
        "index 1..2 100644\n"
        "--- a/n.txt\n"
        "+++ b/n.txt\n"
        "@@ -1 +1 @@\n"
        "-a\n"
        "\\ No newline at end of file\n"
        "+b\n");
    ASSERT_EQ(parsed.lineKinds.size(), 8u);
    EXPECT_TRUE(parsed.lineKinds[6] == DiffLineKind::Context);
    EXPECT_EQ(parsed.files[0].removed, 1);
    EXPECT_EQ(parsed.files[0].added, 1);
}

TEST(DiffText, LastLineWithoutNewlineIsStillCounted) {
    const ParsedPatch parsed = parsePatch("diff --git a/x b/x\n@@ -1 +1 @@\n+added");
    EXPECT_EQ(parsed.lineKinds.size(), 3u);
    EXPECT_EQ(parsed.files[0].added, 1);
}

TEST(DiffText, PathsWithSpacesAreKept) {
    const ParsedPatch parsed = parsePatch("diff --git a/my file.txt b/my file.txt\n");
    ASSERT_EQ(parsed.files.size(), 1u);
    EXPECT_EQ(parsed.files[0].path, std::string("my file.txt"));
}
