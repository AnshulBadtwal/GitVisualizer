cmake_minimum_required(VERSION 3.22)
project(GitVisualizer VERSION 1.0.0 LANGUAGES CXX C)

# Modern C++23 standard enforcement
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Locate Qt 6 modular dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# Locate libgit2 cross-platform (tries CMake config package first, falls back to pkg-config)
find_package(libgit2 CONFIG QUIET)
if(NOT libgit2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBGIT2 REQUIRED libgit2)
endif()

# Enable Qt Meta-Object Compiler (moc), Resource Compiler (rcc), and UI Compiler (uic)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Core Backend and UI Headers
set(BACKEND_HEADERS
    src/core/GitTypes.h
    src/core/GitRepository.h
    src/core/GraphLayoutEngine.h
    src/models/CommitTableModel.h
    src/delegates/GraphLaneDelegate.h
    src/workers/LogWorker.h
    src/workers/CommandRunner.h
    src/ui/CommitGraphView.h
    src/ui/DiffViewerWidget.h
    src/ui/TerminalDockWidget.h
    src/ui/MainWindow.h
)

# Core Backend and UI Implementation Sources
set(BACKEND_SOURCES
    src/core/GitRepository.cpp
    src/core/GraphLayoutEngine.cpp
    src/models/CommitTableModel.cpp
    src/delegates/GraphLaneDelegate.cpp
    src/workers/LogWorker.cpp
    src/workers/CommandRunner.cpp
    src/ui/CommitGraphView.cpp
    src/ui/DiffViewerWidget.cpp
    src/ui/TerminalDockWidget.cpp
    src/ui/MainWindow.cpp
)

# Main Executable Target
add_executable(GitVisualizer
    src/main.cpp
    ${BACKEND_HEADERS}
    ${BACKEND_SOURCES}
)

# Target Include Paths & Linking for libgit2 (handles both CMake target and legacy pkg-config)
if(TARGET libgit2::libgit2)
    target_link_libraries(GitVisualizer PRIVATE libgit2::libgit2)
else()
    target_include_directories(GitVisualizer PRIVATE ${LIBGIT2_INCLUDE_DIRS})
    target_link_libraries(GitVisualizer PRIVATE ${LIBGIT2_LIBRARIES})
endif()

target_include_directories(GitVisualizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(GitVisualizer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Cross-Platform Compiler Warnings and Safety Flags
if(MSVC)
    target_compile_options(GitVisualizer PRIVATE /W4 /permissive-)
else()
    target_compile_options(GitVisualizer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Optional GTest Suite Target Configuration
option(BUILD_TESTS "Build visualizer unit tests using GTest" ON)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(test_graph_engine
        tests/test_graph_engine.cpp
        ${BACKEND_HEADERS}
        ${BACKEND_SOURCES}
    )

    if(TARGET libgit2::libgit2)
        target_link_libraries(test_graph_engine PRIVATE libgit2::libgit2)
    else()
        target_include_directories(test_graph_engine PRIVATE ${LIBGIT2_INCLUDE_DIRS})
        target_link_libraries(test_graph_engine PRIVATE ${LIBGIT2_LIBRARIES})
    endif()

    target_include_directories(test_graph_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GTEST_INCLUDE_DIRS}
    )

    target_link_libraries(test_graph_engine PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        GTest::gtest
        GTest::gtest_main
    )

    if(MSVC)
        target_compile_options(test_graph_engine PRIVATE /W4 /permissive-)
    else()
        target_compile_options(test_graph_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()

    include(GoogleTest)
    gtest_discover_tests(test_graph_engine)
endif()
