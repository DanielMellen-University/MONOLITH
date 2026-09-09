    // Horizontal - buttons must stay visible
    if (r.x + r.w < minButtonSpace) {
        r.x = minButtonSpace - r.w;
    }
    if (r.x > m_logicalWidth - minButtonSpace) {
        r.x = m_logicalWidth - minButtonSpace;
    }

    // Shrink if bigger than current desktop
    if (r.w > m_logicalWidth)  r.w = std::max(Window::MIN_WIDTH, m_logicalWidth);
    if (r.h > usableHeight) r.h = std::max(Window::MIN_HEIGHT + titleH, usableHeight);

    // Final bounds correction
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (r.x + r.w > m_logicalWidth)  r.x = m_logicalWidth - r.w;
    if (r.h <= usableHeight && r.y + r.h > desktopBottom) r.y = desktopBottom - r.h;
    if (r.y + titleH > desktopBottom) r.y = desktopBottom - titleH;
}

// =============================================================================
// Client area helpers
// =============================================================================

bool WindowManager::isInContentArea(const Window& window, int logicalX, int logicalY) const {
    const int titleH = Window::TITLE_BAR_HEIGHT;
    SDL_Rect contentRect = {
        window.rect.x,
        window.rect.y + titleH,
        window.rect.w,
        window.rect.h - titleH
    };
    SDL_Point p = {logicalX, logicalY};
    return SDL_PointInRect(&p, &contentRect);
}

void WindowManager::translateMouseEventToClient(const Window& window, SDL_Event& event) const {
    const int titleH = Window::TITLE_BAR_HEIGHT;
    const int clientOriginX = window.rect.x;
    const int clientOriginY = window.rect.y + titleH;

    if (event.type == SDL_MOUSEMOTION) {
        event.motion.x = screenToLogicalX(event.motion.x) - clientOriginX;
        event.motion.y = screenToLogicalY(event.motion.y) - clientOriginY;
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        event.button.x = screenToLogicalX(event.button.x) - clientOriginX;
        event.button.y = screenToLogicalY(event.button.y) - clientOriginY;
    } else if (event.type == SDL_MOUSEWHEEL) {
        // Wheel events don't have x/y in the same way; apps can use mouse position separately if needed
    }
}

// === Desktop shell launchers (used by Start Menu) ===

// Dynamic per-type instance numbering (see plan and claimNextAppInstanceTitle doc).
// Replaces the previous snapshot-based makeInstanceTitle logic.
std::pair<std::string, int> WindowManager::claimNextAppInstanceTitle(const std::string& base) {
    auto& used = m_activeAppInstances[base];
    int n = 1;
    while (used.count(n)) {
        ++n;
    }
    used.insert(n);

    std::string t = (n == 1 ? base : base + " " + std::to_string(n));
    return {t, n};
}

void WindowManager::compactAppInstances(const std::string& base) {
    if (base.empty()) return;

    // Collect all currently live windows that belong to this app type
    std::vector<Window*> affected;
    for (auto& wp : m_windows) {
        if (wp && wp->appBaseTitle == base) {
            affected.push_back(wp.get());
        }
    }

    if (affected.empty()) {
        m_activeAppInstances.erase(base);
        return;
    }

    // Sort by their *current* instance number. This preserves the relative order
    // the user saw the windows (lower numbers before higher ones).
    std::sort(affected.begin(), affected.end(), [](const Window* a, const Window* b) {
        return a->appInstanceNumber < b->appInstanceNumber;
    });

    // Rebuild the active set and re-title the windows with dense numbers starting at 1
    auto& used = m_activeAppInstances[base];
    used.clear();

    int newN = 1;
    for (Window* w : affected) {
        const std::string newTitle = (newN == 1 ? base : base + " " + std::to_string(newN));

        const bool titleChanged = (w->title != newTitle);
        w->appInstanceNumber = newN;
        w->title = newTitle;

        if (titleChanged) {
            // Force the title texture to be regenerated on next render
            auto cacheIt = m_titleCache.find(w->id);
            if (cacheIt != m_titleCache.end()) {
                if (cacheIt->second.texture) {
                    SDL_DestroyTexture(cacheIt->second.texture);
                }
                m_titleCache.erase(cacheIt);
            }
        }

        used.insert(newN);
        ++newN;
    }
}

void WindowManager::setAppResources(TTF_Font* font, monolith::fs::Filesystem* fs) {
    m_appFont = font;
    m_fs = fs;
}

void WindowManager::launchTerminal() {
    if (!m_appFont) return;

    // Use the dynamic claimer so numbers are per-type and slots are released on close.
    auto [title, inst] = claimNextAppInstanceTitle("Terminal");

    auto app = std::make_unique<monolith::app::TerminalApp>(m_appFont, m_fs);
    int x = 100 + (static_cast<int>(m_windows.size()) % 7) * 35;
    int y = 80 + (static_cast<int>(m_windows.size()) % 5) * 25;
    createWindow(title, x, y, 520, 380, std::move(app), "Terminal", inst);
    // Note: caller (e.g. Start menu click handler) is responsible for closing the menu.
}

void WindowManager::launchTextEditor(const std::string& initialPath) {
    if (!m_appFont) return;

    // Singleton behavior: if we already have an editor open for this exact file, just focus it.
    if (!initialPath.empty() && m_fs) {
        std::string normalized = m_fs->normalize(initialPath);
        auto it = m_fileEditors.find(normalized);
        if (it != m_fileEditors.end() && it->second) {
            bringToFront(it->second);
            return;
        }
    }

    auto app = std::make_unique<monolith::app::TextEditorApp>(m_appFont, m_fs, initialPath);

    std::string title;
    std::string appBaseForTracking;
    int instForTracking = 0;

    if (!initialPath.empty()) {
        size_t slash = initialPath.find_last_of('/');
        std::string baseName = (slash != std::string::npos) ? initialPath.substr(slash + 1) : initialPath;
        if (!baseName.empty()) {
            title = "Editor - " + baseName;
            // File-backed editors keep their descriptive title and do not consume
            // numbers from the bare "Editor" pool (they are already unique by path
            // and protected by the m_fileEditors singleton).
        } else {
            auto [t, n] = claimNextAppInstanceTitle("Editor");
            title = t;
            appBaseForTracking = "Editor";
            instForTracking = n;
        }
    } else {
        auto [t, n] = claimNextAppInstanceTitle("Editor");
        title = t;
        appBaseForTracking = "Editor";
        instForTracking = n;
    }

    int x = 160 + (static_cast<int>(m_windows.size()) % 6) * 30;
    int y = 90 + (static_cast<int>(m_windows.size()) % 4) * 20;
    Window* w = createWindow(title, x, y, 540, 420, std::move(app),
                             appBaseForTracking, instForTracking);

    // Register for singleton tracking if this editor is bound to a specific file
    if (w && !initialPath.empty() && m_fs) {
        associateEditorWithFile(w, initialPath);
    }
    // Note: caller (e.g. Start menu) is responsible for m_showStartMenu = false.
}

void WindowManager::launchFilesystem() {
    if (!m_appFont) return;

    auto app = std::make_unique<monolith::app::FilesystemApp>(m_appFont, m_fs);

    auto [title, inst] = claimNextAppInstanceTitle("Filesystem");

    int x = 200 + (static_cast<int>(m_windows.size()) % 5) * 25;
    int y = 130 + (static_cast<int>(m_windows.size()) % 3) * 18;
    createWindow(title, x, y, 480, 360, std::move(app), "Filesystem", inst);
    // Note: caller (e.g. Start menu) is responsible for m_showStartMenu = false.
}

void WindowManager::launchDrawing(const std::string& initialPath) {
    if (!m_appFont) return;

    if (!initialPath.empty() && m_fs) {
        std::string normalized = m_fs->normalize(initialPath);
        auto it = m_fileDrawings.find(normalized);
        if (it != m_fileDrawings.end() && it->second) {
            bringToFront(it->second);
            return;
        }
    }

    auto app = std::make_unique<monolith::app::DrawingApp>(m_appFont, m_fs, initialPath);

    std::string title;
    std::string appBaseForTracking;
    int instForTracking = 0;

    if (!initialPath.empty() && m_fs) {
        std::string normalized = m_fs->normalize(initialPath);
        size_t slash = normalized.find_last_of('/');
        std::string baseName = (slash != std::string::npos) ? normalized.substr(slash + 1) : normalized;
        title = baseName.empty() ? "Drawing" : ("Drawing - " + baseName);
    } else {
        auto [t, n] = claimNextAppInstanceTitle("Drawing");
        title = t;
        appBaseForTracking = "Drawing";
        instForTracking = n;
    }

    int x = 220 + (static_cast<int>(m_windows.size()) % 6) * 28;
    int y = 100 + (static_cast<int>(m_windows.size()) % 4) * 22;
    Window* w = createWindow(title, x, y, 560, 440, std::move(app),
                             appBaseForTracking, instForTracking);

    if (w && !initialPath.empty() && m_fs) {
        associateDrawingWithFile(w, initialPath);
    }
}

void WindowManager::launchSettings() {
    if (!m_appFont) return;

    auto app = std::make_unique<monolith::app::SettingsApp>(m_appFont, m_fs);
