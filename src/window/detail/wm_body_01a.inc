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
    const int windowId = ptr->id;
    m_windows.push_back(std::move(window));
    clampSingleWindow(*ptr);

    // Attach controller to the app (if any) so it can request window operations
    if (ptr->app) {
        ptr->app->setController(createControllerFor(ptr));

        // Initial size notification
        const int clientH = ptr->rect.h - Window::TITLE_BAR_HEIGHT;
        ptr->app->onResize(ptr->rect.w, clientH > 0 ? clientH : 0);
    }

    // The initial callback can request a controller close. Do not pass the
    // destroyed address into the focus path or return it to the caller.
    if (!isLiveWindow(ptr, windowId)) return nullptr;

    bringToFront(ptr);
    return isLiveWindow(ptr, windowId) ? ptr : nullptr;
}

void WindowManager::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        // Route the host window close through the same dirty-document guard as
        // the Start menu Shut Down action.
        requestQuit();
        return;
    }

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            if (!m_hostFocused) return;
            Window* focusLostWindow = m_focusedWindow;
            const int focusLostWindowId = focusLostWindow ? focusLostWindow->id : -1;
            m_hostFocused = false;

            // SDL may not deliver the matching mouse-up after the host window
            // loses focus. End the client gesture explicitly so Drawing and
            // other drag-based apps cannot remain armed forever.
            Window* capturedWindow = m_mouseCaptureWindow;
            if (capturedWindow && capturedWindow->app) {
                SDL_Event release{};
                release.type = SDL_MOUSEBUTTONUP;
                release.button.button = SDL_BUTTON_LEFT;
                release.button.windowID = event.window.windowID;
                release.button.x = m_mouseX;
                release.button.y = m_mouseY;
                translateMouseEventToClient(*capturedWindow, release);
                capturedWindow->app->handleEvent(release);
            }

            if (m_draggedWindow) {
                m_draggedWindow->beingDragged = false;
                m_draggedWindow = nullptr;
            }
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
            m_mouseCaptureWindow = nullptr;
            m_shellMouseCapture = false;
            m_mouseDown = false;
            m_shellKeyUpsPending.clear();

            // The release callback above may close the focused window and hand
            // logical focus to a survivor. Only the client that was active at
            // the host transition should receive the host-loss notification.
            if (m_focusedWindow == focusLostWindow
                && isLiveWindow(focusLostWindow, focusLostWindowId)
                && focusLostWindow->app && !focusLostWindow->minimized
                && !m_showStartMenu) {
                focusLostWindow->app->onFocusLost();
            }
            endAltTabCycle();
            return;
        }

        if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            if (m_hostFocused) return;
            m_hostFocused = true;
            if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized
                && !m_showStartMenu) {
                m_focusedWindow->app->onFocusGained();
            }
            return;
        }
    }

    if (tryHandleShellHotkeys(event)) {
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;

        // While a client owns the left-button press, keep sending motion to it even
        // outside its content area so drag tools can finish cleanly on release.
        // A shell or frame press has no client owner, so it must not fall through
        // to whichever focused app happens to be underneath the pointer.
        Window* motionWindow = nullptr;
        if (m_mouseDown && m_mouseCaptureWindow) {
            motionWindow = m_mouseCaptureWindow;
        } else if (!m_mouseDown && !m_shellMouseCapture
                   && !m_draggedWindow && !m_resizingWindow) {
            motionWindow = m_focusedWindow;
        }
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
    if (m_focusedWindow && !m_focusedWindow->minimized && m_focusedWindow->app && !m_showStartMenu &&
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT)) {
        m_focusedWindow->app->handleEvent(event);
        // Do not return — WM doesn't consume keyboard yet, but apps get first crack
    }

    // === Global UI first: Taskbar + Start Menu (must be checked even if click misses all windows) ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_shellMouseCapture = false;
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
            clearDesktopIconSelection();
            // Clicking the Start button itself while menu is open should toggle it closed.
            int startBtnScreenWidth = static_cast<int>(70 * m_contentScale);
            int startBtnScreenX = logicalToScreenX(8);
            if (m_mouseX >= startBtnScreenX && m_mouseX < startBtnScreenX + startBtnScreenWidth &&
                m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
                m_shellMouseCapture = true;
                setStartMenuVisible(!m_showStartMenu);
                return;
            }
