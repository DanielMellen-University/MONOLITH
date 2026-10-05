cmake_minimum_required(VERSION 3.16)
project(Monolith VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2
find_package(SDL2 REQUIRED)

# Find SDL2_ttf using pkg-config (more reliable across distros)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf>=2.0.18)

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
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/app/SettingsApp_body_*.inc.z64")
if(NOT MONOLITH_SETTINGS_BODY_Z64)
    message(FATAL_ERROR "No compressed Settings source fragments found")
endif()
set(MONOLITH_SETTINGS_BODY_OUTPUTS)
foreach(MONOLITH_SETTINGS_BODY IN LISTS MONOLITH_SETTINGS_BODY_Z64)
    get_filename_component(MONOLITH_SETTINGS_BODY_NAME "${MONOLITH_SETTINGS_BODY}" NAME)
    string(REGEX REPLACE "\\.z64$" "" MONOLITH_SETTINGS_BODY_NAME "${MONOLITH_SETTINGS_BODY_NAME}")
    list(APPEND MONOLITH_SETTINGS_BODY_OUTPUTS
        "${MONOLITH_SETTINGS_BODIES_DIR}/${MONOLITH_SETTINGS_BODY_NAME}")
endforeach()
add_custom_command(
    OUTPUT ${MONOLITH_SETTINGS_BODY_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MONOLITH_SETTINGS_BODIES_DIR}"
    COMMAND python3 "${CMAKE_SOURCE_DIR}/src/app/decompress_settings_bodies.py"
            "${MONOLITH_SETTINGS_BODIES_DIR}"
    DEPENDS ${MONOLITH_SETTINGS_BODY_Z64}
            "${CMAKE_SOURCE_DIR}/src/app/decompress_settings_bodies.py"
    COMMENT "Decompressing SettingsApp body includes"
    VERBATIM
)
add_custom_target(monolith_settings_bodies DEPENDS ${MONOLITH_SETTINGS_BODY_OUTPUTS})

# Decompress main.cpp body includes (MCP-friendly compressed fragments).
set(MONOLITH_MAIN_BODIES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/main")
file(GLOB MONOLITH_MAIN_BODY_Z64
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/main_body_*.inc.z64")
if(NOT MONOLITH_MAIN_BODY_Z64)
    message(FATAL_ERROR "No compressed main source fragments found")
endif()
set(MONOLITH_MAIN_BODY_OUTPUTS)
foreach(MONOLITH_MAIN_BODY IN LISTS MONOLITH_MAIN_BODY_Z64)
    get_filename_component(MONOLITH_MAIN_BODY_NAME "${MONOLITH_MAIN_BODY}" NAME)
    string(REGEX REPLACE "\\.z64$" "" MONOLITH_MAIN_BODY_NAME "${MONOLITH_MAIN_BODY_NAME}")
    list(APPEND MONOLITH_MAIN_BODY_OUTPUTS
        "${MONOLITH_MAIN_BODIES_DIR}/${MONOLITH_MAIN_BODY_NAME}")
endforeach()
add_custom_command(
    OUTPUT ${MONOLITH_MAIN_BODY_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MONOLITH_MAIN_BODIES_DIR}"
    COMMAND python3 "${CMAKE_SOURCE_DIR}/src/decompress_main_bodies.py"
            "${MONOLITH_MAIN_BODIES_DIR}"
    DEPENDS ${MONOLITH_MAIN_BODY_Z64}
            "${CMAKE_SOURCE_DIR}/src/decompress_main_bodies.py"
    COMMENT "Decompressing main body includes"
    VERBATIM
)
add_custom_target(monolith_main_bodies DEPENDS ${MONOLITH_MAIN_BODY_OUTPUTS})

add_executable(monolith
    src/main.cpp
    src/detail/AtomicFile.cpp
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
    src/app/BreakoutApp.cpp
    src/app/BreakoutLogic.cpp

    # Filesystem
    src/fs/Filesystem.cpp

    # Desktop settings
    src/settings/DesktopSettings.cpp
)

target_include_directories(monolith PRIVATE
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_BINARY_DIR}/generated/settings
    ${CMAKE_CURRENT_BINARY_DIR}/generated/main
)
add_dependencies(monolith monolith_stb_image monolith_settings_bodies monolith_main_bodies)

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

include(CTest)
if(BUILD_TESTING)
    add_test(NAME monolith_headless
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/run_headless_tests.sh")
    set_tests_properties(monolith_headless PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 1800
        ENVIRONMENT "BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}")
endif()
