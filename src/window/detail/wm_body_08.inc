    auto [title, inst] = claimNextAppInstanceTitle("Settings");

    // Position it a bit more to the right/center-ish
    int x = 180 + (static_cast<int>(m_windows.size()) % 4) * 20;
    int y = 110 + (static_cast<int>(m_windows.size()) % 3) * 15;
    createWindow(title, x, y, 480, 460, std::move(app), "Settings", inst);
    // Note: caller (e.g. Start menu) is responsible for m_showStartMenu = false.
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
    if (!window) return;
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

    int restored = 0;
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
        } else if (kind == "editor") {
            launchTextEditor(path);
        } else if (kind == "drawing") {
            launchDrawing(path);
        } else {
            continue;
        }

        if (m_windows.size() > before) {
            applyRestoredGeometry(m_windows.back().get(), x, y, w, h,
                                  minFlag != 0, maxFlag != 0);
            ++restored;
        }
    }
    return restored > 0;
}

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
        if (sym == SDLK_LALT || sym == SDLK_RALT) {
            endAltTabCycle();
        }
        return false;
    }

    if (event.type != SDL_KEYDOWN) return false;

    const SDL_Keysym& key = event.key.keysym;
    const bool alt = (key.mod & KMOD_ALT) != 0;
    const bool ctrl = (key.mod & KMOD_CTRL) != 0;

    if (alt && key.sym == SDLK_TAB) {
        const int dir = (key.mod & KMOD_SHIFT) ? -1 : 1;
        cycleFocus(dir);
        m_showStartMenu = false;
        return true;
    }

    if (ctrl && key.sym == SDLK_ESCAPE) {
        m_showStartMenu = !m_showStartMenu;
        m_startMenuHoverIndex = -1;
        endAltTabCycle();
        return true;
    }

    return false;
}

bool WindowManager::shouldQuit() const {
    return m_quitRequested;
}

void WindowManager::loadDesktopSettings(const std::string& hostPath) {
    m_desktopSettingsHostPath = hostPath;
    m_desktopSettings.loadFromHostPath(hostPath);
    if (m_fs && !m_desktopSettings.wallpaperPath().empty()) {
        const std::string rawWallpaperPath = m_desktopSettings.wallpaperPath();
        const std::string normalizedWallpaperPath = m_fs->normalize(rawWallpaperPath);
        if (rawWallpaperPath != normalizedWallpaperPath) {
            m_desktopSettings.setWallpaperPath(normalizedWallpaperPath);
            m_desktopSettings.saveToHostPath(hostPath);
        }
    }
    applyUiScaleFont();
    // Force wallpaper reload on next render (path may have changed).
    destroyWallpaperTexture();
    m_wallpaperLoadedPath.clear();
}

monolith::settings::RGB WindowManager::getDesktopBackground() const {
    return m_desktopSettings.desktopBackground();
}

void WindowManager::setDesktopBackground(uint8_t r, uint8_t g, uint8_t b) {
    m_desktopSettings.setDesktopBackground({r, g, b});
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

std::string WindowManager::getWallpaperPath() const {
    return m_desktopSettings.wallpaperPath();
}

void WindowManager::setWallpaperPath(const std::string& virtualPath) {
    std::string normalized;
    if (!virtualPath.empty() && m_fs) {
        normalized = m_fs->normalize(virtualPath);
    } else {
        normalized = virtualPath;
    }
    if (normalized == "/") {
        normalized.clear();
    }
    if (m_desktopSettings.wallpaperPath() == normalized) {
        return;
    }
    m_desktopSettings.setWallpaperPath(normalized);
    destroyWallpaperTexture();
    m_wallpaperLoadedPath.clear();
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

void WindowManager::destroyWallpaperTexture() {
    if (m_wallpaperTexture) {
        SDL_DestroyTexture(m_wallpaperTexture);
        m_wallpaperTexture = nullptr;
    }
    m_wallpaperTexW = 0;
    m_wallpaperTexH = 0;
}

void WindowManager::ensureWallpaperTexture(SDL_Renderer* renderer) {
    const std::string& path = m_desktopSettings.wallpaperPath();
    if (path.empty()) {
        if (m_wallpaperTexture) {
            destroyWallpaperTexture();
            m_wallpaperLoadedPath.clear();
        }
        return;
    }
    if (m_wallpaperTexture && m_wallpaperLoadedPath == path) {
        return;
    }

    destroyWallpaperTexture();
    m_wallpaperLoadedPath = path;

    if (!renderer || !m_fs) {
        return;
    }
    if (!m_fs->isFile(path)) {
        return;
    }

    const std::string hostPath = m_fs->toHostPath(path);
    SDL_Surface* surface = loadWallpaperSurface(hostPath);
    if (!surface) {
        // Fail soft: keep solid color background.
        return;
    }

    m_wallpaperTexture = SDL_CreateTextureFromSurface(renderer, surface);
    m_wallpaperTexW = surface->w;
    m_wallpaperTexH = surface->h;
    SDL_FreeSurface(surface);
}

void WindowManager::renderWallpaper(SDL_Renderer* renderer) {
    if (!renderer) return;
    ensureWallpaperTexture(renderer);
    if (!m_wallpaperTexture || m_wallpaperTexW <= 0 || m_wallpaperTexH <= 0) {
        return;
    }

    // Cover-scale: fill the logical desktop, cropping overflow, centered.
    const float scaleX = static_cast<float>(m_logicalWidth) / static_cast<float>(m_wallpaperTexW);
    const float scaleY = static_cast<float>(m_logicalHeight) / static_cast<float>(m_wallpaperTexH);
    const float scale = scaleX > scaleY ? scaleX : scaleY;

    const int drawW = static_cast<int>(m_wallpaperTexW * scale);
    const int drawH = static_cast<int>(m_wallpaperTexH * scale);
    const int logicalX = (m_logicalWidth - drawW) / 2;
    const int logicalY = (m_logicalHeight - drawH) / 2;

    SDL_Rect dst = {
        logicalToScreenX(logicalX),
        logicalToScreenY(logicalY),
        static_cast<int>(drawW * m_contentScale),
        static_cast<int>(drawH * m_contentScale)
    };
    SDL_RenderCopy(renderer, m_wallpaperTexture, nullptr, &dst);
}

bool WindowManager::getClock24Hour() const {
    return m_desktopSettings.clock24Hour();
}

void WindowManager::setClock24Hour(bool enabled) {
    if (m_desktopSettings.clock24Hour() == enabled) {
        return;
    }
    m_desktopSettings.setClock24Hour(enabled);
    // Force the taskbar clock texture to rebuild on the next render.
    if (m_clockTexture) {
        SDL_DestroyTexture(m_clockTexture);
        m_clockTexture = nullptr;
    }
    m_clockText.clear();
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

int WindowManager::getUiScalePercent() const {
    return m_desktopSettings.uiScalePercent();
}

void WindowManager::setUiScalePercent(int percent) {
    if (!monolith::settings::DesktopSettings::isSupportedUiScalePercent(percent)) return;
    if (m_desktopSettings.uiScalePercent() == percent) return;

    m_desktopSettings.setUiScalePercent(percent);
    applyUiScaleFont();
    for (auto& windowPtr : m_windows) {
        if (windowPtr && windowPtr->app) {
            windowPtr->app->onUiScaleChanged();
        }
    }
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}
