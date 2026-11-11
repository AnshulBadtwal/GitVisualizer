#ifndef TEMPREPOFIXTURE_H
#define TEMPREPOFIXTURE_H

// Base class for tests that build a small real repository with the `git` command in a temporary
// folder. Tests skip themselves when `git` is not installed.

#include <gtest/gtest.h>
#include <stdlib.h>   // mkdtemp
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace Git {

namespace fs = std::filesystem;

class TempRepoFixture : public ::testing::Test {
protected:
    fs::path m_dir;

    using Names = std::vector<std::string>;

    void SetUp() override {
        if (std::system("git --version > /dev/null 2>&1") != 0) {
            GTEST_SKIP() << "git is not installed";
        }
        std::string pattern = (fs::temp_directory_path() / "gitviz-test-XXXXXX").string();
        ASSERT_TRUE(mkdtemp(pattern.data()) != nullptr);
        m_dir = pattern;
    }

    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(m_dir, ignored);
    }

    // Runs `git <args>` inside the temporary repository. Messages and names in the tests
    // contain no quotes or spaces that would need escaping beyond the single quotes used here.
    bool git(const std::string& args) const {
        const std::string command =
            "git -C '" + m_dir.string() + "' "
            "-c user.name=Tester -c user.email=tester@example.com "
            "-c commit.gpgsign=false -c tag.gpgsign=false " + args + " > /dev/null 2>&1";
        return std::system(command.c_str()) == 0;
    }

    void initRepo() { ASSERT_TRUE(git("init -q -b main")); }
    void commit(const std::string& message) { ASSERT_TRUE(git("commit --allow-empty -q -m '" + message + "'")); }
};

} // namespace Git

#endif // TEMPREPOFIXTURE_H
