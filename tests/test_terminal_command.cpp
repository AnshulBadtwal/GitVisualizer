#include <gtest/gtest.h>
#include "core/TerminalCommand.h"

using namespace Git;

namespace {

bool isShell(const char *line) {
    return classifyTerminalCommand(line).kind == TerminalCommandKind::Shell;
}

// True if `line` is a cd command for exactly `folder`.
bool isCd(const char *line, const char *folder) {
    const TerminalCommand command = classifyTerminalCommand(line);
    return command.kind == TerminalCommandKind::ChangeDirectory && command.argument == folder;
}

} // namespace

TEST(TerminalCommand, ClearAndClsClearThePanel) {
    EXPECT_TRUE(classifyTerminalCommand("clear").kind == TerminalCommandKind::Clear);
    EXPECT_TRUE(classifyTerminalCommand("  cls  ").kind == TerminalCommandKind::Clear);
}

TEST(TerminalCommand, ClearWithArgumentsGoesToTheShell) {
    EXPECT_TRUE(isShell("clear -x"));
}

TEST(TerminalCommand, OrdinaryCommandsGoToTheShell) {
    EXPECT_TRUE(isShell("git status"));
    EXPECT_TRUE(isShell("ls -la"));
    EXPECT_TRUE(isShell(""));
    EXPECT_TRUE(isShell("cdrom"));          // starts with "cd" but is not the cd command
    EXPECT_TRUE(isShell("cdr foo"));
}

TEST(TerminalCommand, BareCdMeansHome) {
    EXPECT_TRUE(isCd("cd", ""));
    EXPECT_TRUE(isCd("  cd  ", ""));
}

TEST(TerminalCommand, CdWithOnePlainWord) {
    EXPECT_TRUE(isCd("cd src", "src"));
    EXPECT_TRUE(isCd("cd ..", ".."));
    EXPECT_TRUE(isCd("cd -", "-"));
    EXPECT_TRUE(isCd("cd ~", "~"));
    EXPECT_TRUE(isCd("cd ~/projects/app", "~/projects/app"));
    EXPECT_TRUE(isCd("cd   /tmp/x  ", "/tmp/x"));
}

TEST(TerminalCommand, CdWithAQuotedWord) {
    EXPECT_TRUE(isCd("cd 'my folder'", "my folder"));
    EXPECT_TRUE(isCd("cd \"my folder\"", "my folder"));
}

TEST(TerminalCommand, CdThatTheShellWouldInterpretGoesToTheShell) {
    EXPECT_TRUE(isShell("cd a b"));                 // two words
    EXPECT_TRUE(isShell("cd -P src"));              // an option
    EXPECT_TRUE(isShell("cd src && git status"));   // chained command
    EXPECT_TRUE(isShell("cd src; ls"));
    EXPECT_TRUE(isShell("cd $HOME"));               // variable
    EXPECT_TRUE(isShell("cd \"a$b\""));             // variable inside double quotes
    EXPECT_TRUE(isShell("cd 'a'b'"));               // quotes that do not form one word
    EXPECT_TRUE(isShell("cd *"));                   // glob
    EXPECT_TRUE(isShell("cd \"\""));                // empty quoted word
}

TEST(TerminalCommand, SingleQuotesKeepDollarSignsLiteral) {
    EXPECT_TRUE(isCd("cd 'cost$'", "cost$"));
}
