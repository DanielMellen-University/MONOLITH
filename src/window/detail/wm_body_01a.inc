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
    destroyShellTextCache();
    destroyDesktopIconTextCache();
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

void WindowManager::destroyShellTextCache() {
    for (auto& [key, entry] : m_shellTextCache) {
        (void)key;
        if (entry.texture) {
            SDL_DestroyTexture(entry.texture);
            entry.texture = nullptr;
        }
    }
    m_shellTextCache.clear();
}

SDL_Texture* WindowManager::getShellTextTexture(SDL_Renderer* renderer, const char* text,
                                                 SDL_Color color, int& width, int& height) {
    width = 0;
    height = 0;
    if (!renderer || !m_font || !text) return nullptr;

    std::string key(text);
    key.push_back('\0');
    key.push_back(static_cast<char>(color.r));
    key.push_back(static_cast<char>(color.g));
    key.push_back(static_cast<char>(color.b));
    key.push_back(static_cast<char>(color.a));

    auto cached = m_shellTextCache.find(key);
    if (cached != m_shellTextCache.end()) {
        width = cached->second.width;
        height = cached->second.height;
        return cached->second.texture;
    }

    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text, color);
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    ShellTextCacheEntry entry;
    entry.texture = texture;
    entry.width = surface->w;
    entry.height = surface->h;
    width = entry.width;
    height = entry.height;
    SDL_FreeSurface(surface);
    m_shellTextCache.emplace(std::move(key), entry);
    return texture;
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
    window->rect.w = std::max(w, Window::MIN_WIDTH);
    window->rect.h = std::max(h, Window::MIN_HEIGHT + Window::TITLE_BAR_HEIGHT);
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
    invalidateShellHitTargets();
    clampSingleWindow(*ptr);

    // Attach controller to the app (if any) so it can request window operations
    if (ptr->app) {
        ptr->app->setController(createControllerFor(ptr));

        // Initial size notification
        const int clientH = ptr->rect.h - Window::TITLE_BAR_HEIGHT;
        invokeAppCallback([&]() {
            ptr->app->onResize(ptr->rect.w, clientH > 0 ? clientH : 0);
        });
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
                invokeAppCallback([&]() { capturedWindow->app->handleEvent(release); });
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
                invokeAppCallback([&]() { focusLostWindow->app->onFocusLost(); });
            }
            endAltTabCycle();
            return;
        }

        if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            if (m_hostFocused) return;
            m_hostFocused = true;
            if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized
                && !m_showStartMenu) {
                invokeAppCallback([&]() { m_focusedWindow->app->onFocusGained(); });
            }
            return;
        }
    }

    // Ignore queued input while the host window is unfocused, including shell
    // hotkeys. The one exception is dismissing an already-open Start menu: it
    // must be possible to close that modal without resuming its suspended app.
    const bool unfocusedStartDismiss = !m_hostFocused
        && m_showStartMenu
        && event.type == SDL_KEYDOWN
        && event.key.keysym.sym == SDLK_ESCAPE;
    const bool unfocusedStartDismissRelease = !m_hostFocused
        && event.type == SDL_KEYUP
        && event.key.keysym.sym == SDLK_ESCAPE
        && m_shellKeyUpsPending.find(SDLK_ESCAPE) != m_shellKeyUpsPending.end();
    if (!m_hostFocused
        && !unfocusedStartDismiss
        && !unfocusedStartDismissRelease
        && (event.type == SDL_KEYDOWN
            || event.type == SDL_KEYUP
            || event.type == SDL_TEXTINPUT
            || event.type == SDL_MOUSEMOTION
            || event.type == SDL_MOUSEBUTTONDOWN
            || event.type == SDL_MOUSEBUTTONUP
            || event.type == SDL_MOUSEWHEEL)) {
        return;
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
                invokeAppCallback([&]() { motionWindow->app->handleEvent(clientEvent); });
            }
        }
    }

    if (event.type == SDL_MOUSEWHEEL) {
        // SDL2 wheel events do not carry pointer coordinates. Refresh the
        // host position before deriving logical coordinates for wheel routing.
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        m_mouseX = mouseX;
        m_mouseY = mouseY;
    }

    const int logicalMouseX = screenToLogicalX(m_mouseX);
    const int logicalMouseY = screenToLogicalY(m_mouseY);

    // Forward keyboard events (text input, key down/up) to the focused window's app.
    // Suppress while Start menu is open so the menu (mouse-driven) owns the interaction.
    if (m_hostFocused && m_focusedWindow && !m_focusedWindow->minimized
        && m_focusedWindow->app && !m_showStartMenu &&
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT)) {
        invokeAppCallback([&]() { m_focusedWindow->app->handleEvent(event); });
        // A focused app owns client keyboard input. Shell hotkeys were already
        // handled above, so do not let a callback that closes this app route the
        // same event into a newly empty desktop or a selected icon.
        return;
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

        const TaskbarLayout taskbarLayout = computeTaskbarLayout();
        const SDL_Rect taskbarScreenRect = taskbarLayout.taskbarRect;
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;

        // Start Menu popup lives *above* the taskbar, so we must check it for *any* click
        // when it is open (not just clicks whose Y is inside the taskbar strip).
        if (m_showStartMenu) {
            ensureStartMenuHitTargets();
            clearDesktopIconSelection();
            // Clicking the Start button itself while menu is open should toggle it closed.
            SDL_Point startPoint{m_mouseX, m_mouseY};
            if (SDL_PointInRect(&startPoint, &taskbarLayout.startButtonRect)) {
                m_shellMouseCapture = true;
                setStartMenuVisible(!m_showStartMenu);
                return;
            }
