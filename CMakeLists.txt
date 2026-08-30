cmake_minimum_required(VERSION 3.22)
project(GitVisualizer VERSION 1.0.0 LANGUAGES CXX C)

# Modern C++23 standard enforcement
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Locate Qt 6 modular dependencies & system libgit2 on Pop!_OS
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

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

# Target Include Paths
target_include_directories(GitVisualizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBGIT2_INCLUDE_DIRS}
)

# Link Dynamic/Static System Libraries
target_link_libraries(GitVisualizer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    ${LIBGIT2_LIBRARIES}
)

# Compiler Warnings and Safety Flags
target_compile_options(GitVisualizer PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

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

    target_include_directories(test_graph_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LIBGIT2_INCLUDE_DIRS}
        ${GTEST_INCLUDE_DIRS}
    )

    target_link_libraries(test_graph_engine PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        ${LIBGIT2_LIBRARIES}
        GTest::gtest
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(test_graph_engine)
endif()
