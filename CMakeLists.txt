cmake_minimum_required(VERSION 3.16)

project(gomoku-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(gomoku-qt src/main.cpp)

target_link_libraries(gomoku-qt PRIVATE Qt6::Widgets)

set_target_properties(gomoku-qt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
