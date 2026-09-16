    auto [title, inst] = claimNextAppInstanceTitle("Settings");

    // Position it a bit more to the right/center-ish
    int x = 180 + (static_cast<int>(m_windows.size()) % 4) * 20;
    int y = 110 + (static_cast<int>(m_windows.size()) % 3) * 15;
    createWindow(title, x, y, 480, 460, std::move(app), "Settings", inst);
    // Note: caller (e.g. Start menu) is responsible for m_showStartMenu = false;
}

void WindowManager::launchSnake() {
    if (!m_appFont) return;

    auto [title, inst] = claimNextAppInstanceTitle("Snake");
    auto app = std::make_unique<monolith::app::SnakeApp>(m_appFont);
    int x = 140 + (static_cast<int>(m_windows.size()) % 6) * 30;
    int y = 70 + (static_cast<int>(m_windows.size()) % 4) * 25;
    createWindow(title, x, y, 420, 460, std::move(app), "Snake", inst);
}

void WindowManager::launchMinesweeper() {
    if (!m_appFont) return;

    auto [title, inst] = claimNextAppInstanceTitle("Minesweeper");
    auto app = std::make_unique<monolith::app::MinesweeperApp>(m_appFont);
    int x = 160 + (static_cast<int>(m_windows.size()) % 5) * 28;
    int y = 80 + (static_cast<int>(m_windows.size()) % 4) * 22;
    createWindow(title, x, y, 360, 420, std::move(app), "Minesweeper", inst);
}

void WindowManager::launchPong() {
    if (!m_appFont) return;

    auto [title, inst] = claimNextAppInstanceTitle("Pong");
    auto app = std::make_unique<monolith::app::PongApp>(m_appFont);
    int x = 150 + (static_cast<int>(m_windows.size()) % 5) * 26;
    int y = 75 + (static_cast<int>(m_windows.size()) % 4) * 20;
    createWindow(title, x, y, 420, 320, std::move(app), "Pong", inst);
}

void WindowManager::launchBreakout() {
    if (!m_appFont) return;

    auto [title, inst] = claimNextAppInstanceTitle("Breakout");
    auto app = std::make_unique<monolith::app::BreakoutApp>(m_appFont);
    int x = 145 + (static_cast<int>(m_windows.size()) % 5) * 26;
    int y = 70 + (static_cast<int>(m_windows.size()) % 4) * 20;
    createWindow(title, x, y, 420, 360, std::move(app), "Breakout", inst);
}

void WindowManager::openPath(const std::string& virtualPath) {
    if (virtualPath.empty()) return;

    std::string path = m_fs ? m_fs->normalize(virtualPath) : virtualPath;
    if (monolith::app::hasCaseInsensitiveSuffix(path, ".modr")) {
        launchDrawing(path);
    } else {
        launchTextEditor(path);
    }
}

void WindowManager::applyRestoredGeometry(Window* window, int x, int y, int w, int h,
                                          bool minimized, bool maximized) {
    if (!isLiveWindow(window)) return;
    const int windowId = window->id;
    w = std::max(w, Window::MIN_WIDTH);
    h = std::max(h, Window::MIN_HEIGHT + Window::TITLE_BAR_HEIGHT);
    window->rect = {x, y, w, h};
    window->minimized = minimized;
    window->maximized = false;
    if (maximized) {
        window->previousRect = window->rect;
        window->rect = getUsableDesktopRect();
        window->maximized = true;
    }
    clampSingleWindow(*window);
    if (window->app) {
        const int clientH = window->rect.h - Window::TITLE_BAR_HEIGHT;
        window->app->onResize(window->rect.w, clientH > 0 ? clientH : 0);
    }

    // Restored apps can reject or close themselves from their first geometry
    // callback. Do not inspect the old address after that callback returns.
    if (!isLiveWindow(window, windowId)) return;

    // A session entry is created focused, then its saved minimized state is
    // applied. Do not leave keyboard focus attached to that hidden window.
    if (window->minimized && m_focusedWindow == window) {
        focusTopmostVisibleWindow();
    }
}

bool WindowManager::loadSession(const std::string& hostPath) {
    std::ifstream in(hostPath);
    if (!in) return false;

    std::string header;
    if (!std::getline(in, header)) return false;
    // Trim CR
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header != "session_v1") return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string kind;
        int x = 0, y = 0, w = 400, h = 300, minFlag = 0, maxFlag = 0;
        std::string path;
        if (!(iss >> kind >> x >> y >> w >> h >> minFlag >> maxFlag)
            || !monolith::window::session::readPath(iss, path)) {
            continue;
        }

        const bool expectsEditorFile = kind == "editor" && !path.empty();
        const bool expectsDrawingFile = kind == "drawing" && !path.empty();
        const bool expectsFile = expectsEditorFile || expectsDrawingFile;
        if (expectsFile && (!m_fs || !m_fs->isFile(path))) {
            // A file-backed session entry cannot be faithfully restored as a
            // blank untitled app. Leave the stale entry out of this session.
            continue;
        }

        const size_t before = m_windows.size();
        if (kind == "terminal") {
            launchTerminal();
        } else if (kind == "filesystem") {
            launchFilesystem();
        } else if (kind == "settings") {
            launchSettings();
        } else if (kind == "snake") {
            launchSnake();
        } else if (kind == "minesweeper") {
            launchMinesweeper();
        } else if (kind == "pong") {
            launchPong();
        } else if (kind == "breakout") {
            launchBreakout();
        } else if (kind == "editor") {
            launchTextEditor(path);
        } else if (kind == "drawing") {
            launchDrawing(path);
        } else {
            continue;
        }

        if (m_windows.size() > before) {
            Window* restoredWindow = m_windows.back().get();
            const bool boundSuccessfully =
                (!expectsEditorFile && !expectsDrawingFile)
                || (expectsEditorFile && !restoredWindow->editedFilePath.empty())
                || (expectsDrawingFile && !restoredWindow->drawingFilePath.empty());
            if (!boundSuccessfully) {
                closeWindow(restoredWindow);
                continue;
            }
            applyRestoredGeometry(restoredWindow, x, y, w, h,
                                  minFlag != 0, maxFlag != 0);
        }
    }
    // A valid session can intentionally contain no restorable windows, for
    // example after the user closed everything before shutting down. Report
    // that the session was accepted so startup does not seed demo windows.
    return true;
}
