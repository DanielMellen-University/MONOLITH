cmake_minimum_required(VERSION 3.16)
project(Monolith VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2
find_package(SDL2 REQUIRED)

# Find SDL2_ttf using pkg-config (more reliable across distros)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(monolith
    src/main.cpp
    src/window/WindowManager.cpp

    # App implementations
    src/app/TerminalApp.cpp
    src/app/TextEditorApp.cpp
    src/app/FilesystemApp.cpp
    src/app/SettingsApp.cpp
    src/app/DrawingApp.cpp

    # Filesystem
    src/fs/Filesystem.cpp
)

target_include_directories(monolith PRIVATE ${SDL2_TTF_INCLUDE_DIRS})

target_link_libraries(monolith
    PRIVATE
        SDL2::SDL2
        ${SDL2_TTF_LIBRARIES}
)

# Helpful warnings
if(MSVC)
    target_compile_options(monolith PRIVATE /W4)
else()
    target_compile_options(monolith PRIVATE -Wall -Wextra -Wpedantic)
endif()
