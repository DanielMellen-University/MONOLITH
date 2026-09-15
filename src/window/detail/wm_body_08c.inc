
bool WindowManager::saveSession(const std::string& hostPath) const {
    std::ofstream out(hostPath, std::ios::trunc);
    if (!out) return false;

    out << "session_v1\n";
    out << "# kind x y w h minimized maximized quoted-path\n";

    for (const auto& winPtr : m_windows) {
        if (!winPtr) continue;
        const Window& w = *winPtr;

        std::string kind;
        std::string path = "-";

        if (!w.editedFilePath.empty()) {
            kind = "editor";
            path = w.editedFilePath;
        } else if (!w.drawingFilePath.empty()) {
            kind = "drawing";
            path = w.drawingFilePath;
        } else if (w.appBaseTitle == "Terminal") {
            kind = "terminal";
        } else if (w.appBaseTitle == "Filesystem") {
            kind = "filesystem";
        } else if (w.appBaseTitle == "Settings") {
            kind = "settings";
        } else if (w.appBaseTitle == "Snake") {
            kind = "snake";
        } else if (w.appBaseTitle == "Minesweeper") {
            kind = "minesweeper";
        } else if (w.appBaseTitle == "Pong") {
            kind = "pong";
        } else if (w.appBaseTitle == "Breakout") {
            kind = "breakout";
        } else if (w.appBaseTitle == "Editor") {
            kind = "editor";
        } else if (w.appBaseTitle == "Drawing") {
            kind = "drawing";
        } else {
            continue; // unknown / placeholder windows are not restored
        }

        // Prefer pre-maximize geometry when maximized so restore is faithful.
        SDL_Rect r = w.maximized ? w.previousRect : w.rect;
        if (r.w <= 0 || r.h <= 0) r = w.rect;

        out << kind << ' '
            << r.x << ' ' << r.y << ' ' << r.w << ' ' << r.h << ' '
            << (w.minimized ? 1 : 0) << ' '
            << (w.maximized ? 1 : 0) << ' ';
        monolith::window::session::writePath(out, path);
        out << '\n';
    }
    return static_cast<bool>(out);
}

void WindowManager::requestQuit() {
    if (m_quitRequested) return;

    // Shut Down is still a close operation. Ask every app in this request so
    // multiple dirty documents are armed together instead of serializing them.
    bool allAllowed = true;
    for (const auto& window : m_windows) {
        if (window && window->app && !window->app->allowClose()) {
            allAllowed = false;
        }
    }
    if (!allAllowed) return;

    m_quitRequested = true;
    // Menu close is handled by the Start menu click handler (or caller).
}

void WindowManager::endAltTabCycle() {
    m_altTabCycling = false;
    m_altTabIndex = 0;
    m_altTabOrder.clear();
}

void WindowManager::setStartMenuVisible(bool visible) {
    if (m_showStartMenu == visible) return;

    if (visible) {
        m_showStartMenu = true;
        m_startMenuSuspendedWindow = nullptr;
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized) {
            m_startMenuSuspendedWindow = m_focusedWindow;
            m_startMenuSuspendedWindow->app->onFocusLost();
        }
        return;
    }

    Window* suspendedWindow = m_startMenuSuspendedWindow;
    m_startMenuSuspendedWindow = nullptr;
    m_showStartMenu = false;
    if (m_hostFocused && suspendedWindow && suspendedWindow == m_focusedWindow
        && suspendedWindow->app && !suspendedWindow->minimized) {
        suspendedWindow->app->onFocusGained();
    }
}

void WindowManager::cycleFocus(int direction) {
    if (m_windows.empty()) return;

    if (!m_altTabCycling) {
        m_altTabOrder.clear();
        for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
            if (*it) m_altTabOrder.push_back(it->get());
        }
        m_altTabIndex = 0;
        m_altTabCycling = true;
    }

    const int n = static_cast<int>(m_altTabOrder.size());
    if (n == 0) return;

    // Drop any windows that were closed (should not happen mid-cycle after closeWindow ends it).
    m_altTabIndex = (m_altTabIndex + direction % n + n) % n;
    Window* target = m_altTabOrder[static_cast<size_t>(m_altTabIndex)];
    if (!target) return;
    if (target->minimized) {
        target->minimized = false;
    }
    bringToFront(target);
}

bool WindowManager::tryHandleShellHotkeys(const SDL_Event& event) {
    if (event.type == SDL_KEYUP) {
        const SDL_Keycode sym = event.key.keysym.sym;
        const auto pending = m_shellKeyUpsPending.find(sym);
        if (pending != m_shellKeyUpsPending.end()) {
            m_shellKeyUpsPending.erase(pending);
            return true;
        }
        if (sym == SDLK_LALT || sym == SDLK_RALT) {
            endAltTabCycle();
            // Alt is the shell-owned modifier for Alt+Tab. Consume its
            // release too so the complete gesture stays outside client apps.
            return true;
        }
        return false;
    }

    if (event.type != SDL_KEYDOWN) return false;

    const SDL_Keysym& key = event.key.keysym;
    const bool alt = (key.mod & KMOD_ALT) != 0;
    const bool ctrl = (key.mod & KMOD_CTRL) != 0;

    if (alt && key.sym == SDLK_TAB) {
        setStartMenuVisible(false);
        const int dir = (key.mod & KMOD_SHIFT) ? -1 : 1;
        cycleFocus(dir);
        m_shellKeyUpsPending.insert(SDLK_TAB);
        clearDesktopIconSelection();
        return true;
    }

    if (ctrl && key.sym == SDLK_ESCAPE) {
        setStartMenuVisible(!m_showStartMenu);
        m_shellKeyUpsPending.insert(SDLK_ESCAPE);
        clearDesktopIconSelection();
        m_startMenuHoverIndex = -1;
        endAltTabCycle();
        return true;
    }

    return false;
}

bool WindowManager::shouldQuit() const {
    return m_quitRequested;
}
