#!/usr/bin/env bash
# Compile and run the complete documented headless verification suite.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
CXX="${CXX:-c++}"

cd "$ROOT"
mkdir -p "$BUILD_DIR"

read -r -a SDL_FLAGS <<< "$(pkg-config --cflags --libs sdl2)"
read -r -a SDL_TTF_FLAGS <<< "$(pkg-config --cflags --libs sdl2 SDL2_ttf)"

compile_plain() {
    local name="$1"
    shift
    echo "[build] $name"
    "$CXX" -std=c++23 "$@" -o "$BUILD_DIR/$name"
}

compile_sdl() {
    local name="$1"
    shift
    compile_plain "$name" "$@" "${SDL_TTF_FLAGS[@]}"
}

compile_window_manager() {
    local name="$1"
    shift
    compile_plain "$name" \
        -I"$BUILD_DIR/generated" \
        -I"$BUILD_DIR/generated/settings" \
        "$@" \
        "$ROOT/src/window/WindowManager.cpp" \
        "$ROOT/src/window/WallpaperImage.cpp" \
        "$ROOT/src/app/"*.cpp \
        "$ROOT/src/fs/Filesystem.cpp" \
        "$ROOT/src/settings/DesktopSettings.cpp" \
        "${SDL_TTF_FLAGS[@]}"
}

run_plain() {
    local name="$1"
    echo "[run] $name"
    "$BUILD_DIR/$name"
}

run_sdl() {
    local name="$1"
    echo "[run] $name"
    SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
    SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" \
        "$BUILD_DIR/$name"
}

echo "[check] static integration"
./scripts/verify_drawing_integration.sh
./scripts/verify_games_integration.sh
./scripts/verify_desktop_icons.sh

echo "[build] generated sources"
cmake --build "$BUILD_DIR" --target monolith_settings_bodies monolith_stb_image

compile_plain test_file_path scripts/test_file_path.cpp
compile_plain test_utf8 scripts/test_utf8.cpp
compile_plain test_random scripts/test_random.cpp
compile_plain test_tick_math scripts/test_tick_math.cpp
compile_plain test_terminal_lexer scripts/test_terminal_lexer.cpp src/app/TerminalLexer.cpp
compile_sdl test_terminal_filesystem_state \
    scripts/test_terminal_filesystem_state.cpp src/app/TerminalApp.cpp \
    src/app/TerminalLexer.cpp src/fs/Filesystem.cpp
compile_sdl test_text_editor_state \
    scripts/test_text_editor_state.cpp src/app/TextEditorApp.cpp src/fs/Filesystem.cpp
compile_sdl test_filesystem_app_state \
    scripts/test_filesystem_app_state.cpp src/app/FilesystemApp.cpp src/fs/Filesystem.cpp
compile_plain test_fs_roadmap scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp
compile_plain test_drawing_roadmap scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp
compile_sdl test_drawing_state \
    scripts/test_drawing_state.cpp src/app/DrawingApp.cpp \
    src/app/DrawingRaster.cpp src/fs/Filesystem.cpp
compile_plain test_modr_format scripts/test_modr_format.cpp
compile_plain test_desktop_settings scripts/test_desktop_settings.cpp src/settings/DesktopSettings.cpp
compile_sdl test_settings_app_state \
    -I"$BUILD_DIR/generated/settings" scripts/test_settings_app_state.cpp \
    src/app/SettingsApp.cpp src/fs/Filesystem.cpp
compile_plain test_session_format scripts/test_session_format.cpp
compile_sdl test_snake_state scripts/test_snake_state.cpp src/app/SnakeApp.cpp
compile_sdl test_minesweeper_state scripts/test_minesweeper_state.cpp src/app/MinesweeperApp.cpp
compile_plain test_pong_state scripts/test_pong_state.cpp src/app/PongLogic.cpp
compile_plain test_breakout_state scripts/test_breakout_state.cpp src/app/BreakoutLogic.cpp
compile_plain test_desktop_icons scripts/test_desktop_icons.cpp "${SDL_FLAGS[@]}"
compile_window_manager test_window_file_open scripts/test_window_file_open.cpp
compile_window_manager test_window_coordinates scripts/test_window_coordinates.cpp
compile_window_manager test_window_mouse_capture scripts/test_window_mouse_capture.cpp
compile_window_manager test_window_focus scripts/test_window_focus.cpp
compile_window_manager test_window_quit scripts/test_window_quit.cpp

run_plain test_file_path
run_plain test_utf8
run_plain test_random
run_plain test_tick_math
run_plain test_terminal_lexer
run_sdl test_terminal_filesystem_state
run_sdl test_text_editor_state
run_sdl test_filesystem_app_state
run_plain test_fs_roadmap
run_plain test_drawing_roadmap
run_sdl test_drawing_state
run_plain test_modr_format
run_plain test_desktop_settings
run_sdl test_settings_app_state
run_plain test_session_format
run_sdl test_snake_state
run_sdl test_minesweeper_state
run_plain test_pong_state
run_plain test_breakout_state
run_sdl test_desktop_icons
run_sdl test_window_file_open
run_sdl test_window_coordinates
run_sdl test_window_mouse_capture
run_sdl test_window_focus
run_sdl test_window_quit

echo "ALL HEADLESS TESTS PASSED"
