#ifndef TERMINALCOMMAND_H
#define TERMINALCOMMAND_H

#include <string>
#include <string_view>

// What the embedded terminal does with a line the person typed. Every command runs in its own
// shell process, so some lines must be handled by the panel itself, because a process cannot
// change the folder or clear the output of the panel that started it:
//
//     clear / cls         empty the output panel
//     cd, cd <folder>     change the panel's working folder (only the plain forms: one word,
//                         optionally in quotes, nothing the shell would interpret)
//
// Everything else, including "cd a && git status", goes to the shell as it is.
// Qt-free, so it is unit-tested on its own.

namespace Git {

enum class TerminalCommandKind { Shell, Clear, ChangeDirectory };

struct TerminalCommand {
    TerminalCommandKind kind{TerminalCommandKind::Shell};
    std::string argument;   // ChangeDirectory only: the folder as typed, quotes removed; empty = home
};

[[nodiscard]] TerminalCommand classifyTerminalCommand(std::string_view line);

} // namespace Git

#endif // TERMINALCOMMAND_H
