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

# Fetch pinned stb_image.h for PNG/JPEG wallpaper decode.
set(MONOLITH_STB_IMAGE_AMALGAM "${CMAKE_CURRENT_BINARY_DIR}/generated/stb_image.h")
set(MONOLITH_STB_IMAGE_SHA256 "594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3")
add_custom_command(
    OUTPUT "${MONOLITH_STB_IMAGE_AMALGAM}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND python3 "${CMAKE_SOURCE_DIR}/third_party/stb/amalgamate.py"
            "${MONOLITH_STB_IMAGE_AMALGAM}" "${MONOLITH_STB_IMAGE_SHA256}"
    DEPENDS "${CMAKE_SOURCE_DIR}/third_party/stb/amalgamate.py"
    COMMENT "Fetching pinned stb_image.h"
    VERBATIM
)
add_custom_target(monolith_stb_image DEPENDS "${MONOLITH_STB_IMAGE_AMALGAM}")

# Decompress SettingsApp body includes (MCP-friendly compressed fragments).
set(MONOLITH_SETTINGS_BODIES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/settings")
file(GLOB MONOLITH_SETTINGS_BODY_Z64
    "${CMAKE_SOURCE_DIR}/src/app/SettingsApp_body_*.inc.z64")
add_custom_command(
    OUTPUT "${MONOLITH_SETTINGS_BODIES_DIR}/SettingsApp_body_00.inc"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MONOLITH_SETTINGS_BODIES_DIR}"
    COMMAND python3 "${CMAKE_SOURCE_DIR}/src/app/decompress_settings_bodies.py"
            "${MONOLITH_SETTINGS_BODIES_DIR}"
    DEPENDS ${MONOLITH_SETTINGS_BODY_Z64}
            "${CMAKE_SOURCE_DIR}/src/app/decompress_settings_bodies.py"
    COMMENT "Decompressing SettingsApp body includes"
    VERBATIM
)
add_custom_target(monolith_settings_bodies DEPENDS "${MONOLITH_SETTINGS_BODIES_DIR}/SettingsApp_body_00.inc")

add_executable(monolith
    src/main.cpp
    src/window/WindowManager.cpp
    src/window/WallpaperImage.cpp

    # App implementations
    src/app/TerminalApp.cpp
    src/app/TerminalLexer.cpp
    src/app/TextEditorApp.cpp
    src/app/FilesystemApp.cpp
    src/app/SettingsApp.cpp
    src/app/DrawingApp.cpp
    src/app/DrawingRaster.cpp
    src/app/SnakeApp.cpp
    src/app/MinesweeperApp.cpp
    src/app/PongApp.cpp
    src/app/PongLogic.cpp

    # Filesystem
    src/fs/Filesystem.cpp

    # Desktop settings
    src/settings/DesktopSettings.cpp
)

target_include_directories(monolith PRIVATE
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_BINARY_DIR}/generated/settings
)
add_dependencies(monolith monolith_stb_image monolith_settings_bodies)

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
