constexpr int kTaskbarHeight = 28;
constexpr int kWindowButtonSize = 16;
constexpr int kWindowButtonSpacing = 6;
constexpr int kWindowButtonRightPadding = 10;
constexpr int kMinimumVisibleButtonSpace = 80;
constexpr int kBaseFontPointSize = 14;
}

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() {
    for (auto& [id, entry] : m_titleCache) {
        (void)id;
        if (entry.texture) {
            SDL_DestroyTexture(entry.texture);
            entry.texture = nullptr;
        }

    }
    m_titleCache.clear();
    destroyClockTextures();
    destroyWallpaperTexture();
}

void WindowManager::destroyClockTextures() {
    if (m_clockTexture) {
        SDL_DestroyTexture(m_clockTexture);
        m_clockTexture = nullptr;
    }
    m_clockText.clear();
    m_clockTexW = 0;
    m_clockTexH = 0;
    if (m_clockDateTexture) {
        SDL_DestroyTexture(m_clockDateTexture);
        m_clockDateTexture = nullptr;
    }
    m_clockDateText.clear();
    m_clockDateTexW = 0;
    m_clockDateTexH = 0;
}

Window* WindowManager::createWindow(const std::string& title, int x, int y, int w, int h,
                                        std::unique_ptr<monolith::app::App> app,
                                        const std::string& appBase,
                                        int instanceNumber) {
    auto window = std::make_unique<Window>();
    window->id = m_nextId++;
    window->title = title;
    window->rect.x = x;
    window->rect.y = y;
    window->rect.w = w;
    window->rect.h = h;
    window->app = std::move(app);

    // Record instance tracking metadata (if provided by a launcher via claimNext...).
    // This is what allows closeWindow to release the slot for reuse.
    if (!appBase.empty() && instanceNumber > 0) {
        window->appBaseTitle = appBase;
        window->appInstanceNumber = instanceNumber;
    }

    Window* ptr = window.get();
    m_windows.push_back(std::move(window));
    clampSingleWindow(*ptr);

    // Attach controller to the app (if any) so it can request window operations
    if (ptr->app) {
        ptr->app->setController(createControllerFor(ptr));

        // Initial size notification
        const int clientH = ptr->rect.h - Window::TITLE_BAR_HEIGHT;
        ptr->app->onResize(ptr->rect.w, clientH > 0 ? clientH : 0);
    }

    bringToFront(ptr);
    return ptr;
}

void WindowManager::handleEvent(const SDL_Event& event) {
    if (tryHandleShellHotkeys(event)) {
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;

        // While a client owns the left-button press, keep sending motion to it even
        // outside its content area so drag tools can finish cleanly on release.
        Window* motionWindow = (m_mouseDown && m_mouseCaptureWindow)
            ? m_mouseCaptureWindow
            : m_focusedWindow;
        const bool capturedMotion = motionWindow && motionWindow == m_mouseCaptureWindow;
        if (motionWindow && motionWindow->app && !motionWindow->minimized
            && (capturedMotion || !m_showStartMenu)) {
            const int logicalX = screenToLogicalX(m_mouseX);
            const int logicalY = screenToLogicalY(m_mouseY);
            if (capturedMotion || isInContentArea(*motionWindow, logicalX, logicalY)) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*motionWindow, clientEvent);
                motionWindow->app->handleEvent(clientEvent);
            }
        }
    }

    const int logicalMouseX = screenToLogicalX(m_mouseX);
    const int logicalMouseY = screenToLogicalY(m_mouseY);

    // Forward keyboard events (text input, key down/up) to the focused window's app.
    // Suppress while Start menu is open so the menu (mouse-driven) owns the interaction.
    if (m_focusedWindow && m_focusedWindow->app && !m_showStartMenu &&
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT)) {
        m_focusedWindow->app->handleEvent(event);
        // Do not return — WM doesn't consume keyboard yet, but apps get first crack
    }

    // === Global UI first: Taskbar + Start Menu (must be checked even if click misses all windows) ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (!m_mouseDown) {
            m_mouseCaptureWindow = nullptr;
        }
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        const SDL_Rect taskbarScreenRect = logicalRectToScreen(getTaskbarRect());
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;

        // Start Menu popup lives *above* the taskbar, so we must check it for *any* click
        // when it is open (not just clicks whose Y is inside the taskbar strip).
        if (m_showStartMenu) {
            // Clicking the Start button itself while menu is open should toggle it closed.
            int startBtnScreenWidth = static_cast<int>(70 * m_contentScale);
            int startBtnScreenX = logicalToScreenX(8);
            if (m_mouseX >= startBtnScreenX && m_mouseX < startBtnScreenX + startBtnScreenWidth &&
                m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
                m_showStartMenu = !m_showStartMenu;
                return;
            }

            // Check for clicks directly on menu items (the actionable rows)
            for (const auto& item : m_startMenuItems) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &item.rect)) {
                    switch (item.action) {
                        case 0: launchTerminal(); break;
                        case 1: launchTextEditor(); break;
                        case 2: launchFilesystem(); break;
                        case 3: launchSettings(); break;
                        case 4: launchDrawing(); break;
                        case 5: launchSnake(); break;
                        case 6: launchMinesweeper(); break;
                        case 7: launchPong(); break;
                        case 8: requestQuit(); break;
                        default: break;
                    }
                    m_showStartMenu = false;
                    return;
                }
            }

            // Click inside the menu rect (header or background, not on an item) → consume, keep menu open
            SDL_Point p = {m_mouseX, m_mouseY};
            if (SDL_PointInRect(&p, &m_startMenuRect)) {
                return;
            }

            // Click was completely outside the menu → close it (but do not return yet,
            // so the click can still be processed by windows or other UI below).
            m_showStartMenu = false;
        }

        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
            // Start button (when menu was not open, or after we handled the open case above)
            int startBtnScreenWidth = static_cast<int>(70 * m_contentScale);
            int startBtnScreenX = logicalToScreenX(8);

            if (m_mouseX >= startBtnScreenX && m_mouseX < startBtnScreenX + startBtnScreenWidth) {
                m_showStartMenu = !m_showStartMenu;
                return;
            }

            // Scroll arrows (rects populated during render). Must run before the
            // unconditional taskbar return — a separate post-return handler never fired.
            if (m_taskbarNeedsScroll) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &m_taskbarLeftArrowRect)) {
                    m_taskbarScrollOffset = std::max(0, m_taskbarScrollOffset - 120);
                    m_showStartMenu = false;
                    return;
                }
                if (SDL_PointInRect(&p, &m_taskbarRightArrowRect)) {
                    // Soft upper bound; drawing clamps which buttons stay visible.
                    m_taskbarScrollOffset += 120;
                    m_showStartMenu = false;
                    return;
                }
            }

            // Taskbar window buttons (populated during render)
            for (const auto& entry : m_taskbarEntries) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &entry.rect)) {
                    if (entry.window) {
                        if (entry.window->minimized) {
                            entry.window->minimized = false;
                            bringToFront(entry.window);
                        } else if (entry.window == m_focusedWindow) {
                            // Clicking the active window's taskbar button → minimize it (XP-like toggle)
                            entry.window->minimized = true;
                        } else {
                            bringToFront(entry.window);
                        }
                    }
                    m_showStartMenu = false;
                    return;
                }
            }

            m_showStartMenu = false;
            return;
        }
    }

    // Mouse wheel over taskbar scrolls the window buttons
    if (event.type == SDL_MOUSEWHEEL) {
        const SDL_Rect taskbarScreenRect = logicalRectToScreen(getTaskbarRect());
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;
        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom && m_taskbarNeedsScroll) {
            m_taskbarScrollOffset += (event.wheel.y > 0 ? -80 : 80);
            if (m_taskbarScrollOffset < 0) m_taskbarScrollOffset = 0;
            // Upper bound will be soft-limited by drawing (buttons just stop appearing)
            return;
        }
