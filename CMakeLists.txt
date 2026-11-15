cmake_minimum_required(VERSION 3.22)
project(GitVisualizer VERSION 1.0.0 LANGUAGES CXX C)

# Modern C++23 standard enforcement
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Locate Qt 6 modular dependencies & system libgit2 on Pop!_OS
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)

# Enable Qt Meta-Object Compiler (moc), Resource Compiler (rcc), and UI Compiler (uic)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# GitVisualizerCore: everything except main(). Both the app and the tests
# link against this library, so each source file is compiled only once.
# ---------------------------------------------------------------------------
set(CORE_HEADERS
    src/core/CustomContainers.h
    src/core/GitTypes.h
    src/core/OidCache.h
    src/core/DiffText.h
    src/core/CommitSearch.h
    src/core/NavigationHistory.h
    src/core/TerminalCommand.h
    src/core/GitRefs.h
    src/core/GitRepository.h
    src/core/GitWatch.h
    src/core/GraphLayoutEngine.h
    src/models/CommitTableModel.h
    src/delegates/GraphLaneDelegate.h
    src/workers/LogWorker.h
    src/workers/CommandRunner.h
    src/workers/DetailsWorker.h
    src/ui/CommitGraphView.h
    src/ui/CommitDetailsWidget.h
    src/ui/SearchBar.h
    src/ui/DiffViewerWidget.h
    src/ui/TerminalDockWidget.h
    src/ui/MainWindow.h
)

set(CORE_SOURCES
    src/core/DiffText.cpp
    src/core/CommitSearch.cpp
    src/core/NavigationHistory.cpp
    src/core/TerminalCommand.cpp
    src/core/GitRefs.cpp
    src/core/GitRepository.cpp
    src/core/GitWatch.cpp
    src/core/GraphLayoutEngine.cpp
    src/models/CommitTableModel.cpp
    src/delegates/GraphLaneDelegate.cpp
    src/workers/LogWorker.cpp
    src/workers/CommandRunner.cpp
    src/workers/DetailsWorker.cpp
    src/ui/CommitGraphView.cpp
    src/ui/CommitDetailsWidget.cpp
    src/ui/SearchBar.cpp
    src/ui/DiffViewerWidget.cpp
    src/ui/TerminalDockWidget.cpp
    src/ui/MainWindow.cpp
)

add_library(GitVisualizerCore STATIC ${CORE_HEADERS} ${CORE_SOURCES})

target_include_directories(GitVisualizerCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(GitVisualizerCore PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    PkgConfig::LIBGIT2
)

# Compiler Warnings and Safety Flags
target_compile_options(GitVisualizerCore PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(GitVisualizer src/main.cpp)

target_link_libraries(GitVisualizer PRIVATE GitVisualizerCore)

target_compile_options(GitVisualizer PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

# ---------------------------------------------------------------------------
# Optional GTest suite. If GTest is missing, configuration still succeeds.
# Install on Pop!_OS / Ubuntu:  sudo apt install libgtest-dev
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build visualizer unit tests using GTest" ON)

if(BUILD_TESTS)
    find_package(GTest)

    if(GTest_FOUND)
        enable_testing()

        add_executable(test_graph_engine tests/test_graph_engine.cpp)

        target_link_libraries(test_graph_engine PRIVATE
            GitVisualizerCore
            GTest::gtest
            GTest::gtest_main
        )

        add_executable(test_git_data
            tests/test_git_refs.cpp
            tests/test_git_repository.cpp
            tests/test_git_watch.cpp
            tests/test_oid_cache.cpp
            tests/test_diff_text.cpp
            tests/test_commit_search.cpp
            tests/test_navigation_history.cpp
            tests/test_terminal_command.cpp
        )

        target_link_libraries(test_git_data PRIVATE
            GitVisualizerCore
            GTest::gtest
            GTest::gtest_main
        )

        include(GoogleTest)
        gtest_discover_tests(test_graph_engine)
        gtest_discover_tests(test_git_data)
    else()
        message(WARNING "GTest not found: unit tests are disabled. "
                        "Install it with 'sudo apt install libgtest-dev' "
                        "or configure with -DBUILD_TESTS=OFF.")
    endif()
endif()
