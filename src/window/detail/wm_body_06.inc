    const int left   = window.rect.x;
    const int right  = window.rect.x + window.rect.w;
    const int top    = window.rect.y;
    const int bottom = window.rect.y + window.rect.h;

    bool onLeft   = mouseX >= left && mouseX <= left + border;
    bool onRight  = mouseX >= right - border && mouseX <= right;
    bool onTop    = mouseY >= top && mouseY <= top + border;
    bool onBottom = mouseY >= bottom - border && mouseY <= bottom;

    if (onLeft && onTop)     return ResizeDirection::TopLeft;
    if (onRight && onTop)    return ResizeDirection::TopRight;
    if (onLeft && onBottom)  return ResizeDirection::BottomLeft;
    if (onRight && onBottom) return ResizeDirection::BottomRight;

    if (onLeft)   return ResizeDirection::Left;
    if (onRight)  return ResizeDirection::Right;
    if (onTop)    return ResizeDirection::Top;
    if (onBottom) return ResizeDirection::Bottom;

    return ResizeDirection::None;
}

void WindowManager::applyResize(Window* window, int mouseX, int mouseY) {
    if (!window || m_resizeDirection == ResizeDirection::None) return;

    const int minW = Window::MIN_WIDTH;
    const int minH = Window::MIN_HEIGHT + Window::TITLE_BAR_HEIGHT;

    SDL_Rect& r = window->rect;

    // Compute new dimensions while respecting minimums.
    // This prevents the window edge from ever moving past the min size
    // (the edge "stops" even if the user keeps dragging the mouse).
    int newX = r.x;
    int newY = r.y;
    int newW = r.w;
    int newH = r.h;

    switch (m_resizeDirection) {
        case ResizeDirection::Left: {
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            break;
        }
        case ResizeDirection::Right: {
            int tentativeW = mouseX - r.x;
            newW = std::max(minW, tentativeW);
            break;
        }
        case ResizeDirection::Top: {
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::Bottom: {
            int tentativeH = mouseY - r.y;
            newH = std::max(minH, tentativeH);
            break;
        }
        case ResizeDirection::TopLeft: {
            // Horizontal
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            // Vertical
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::TopRight: {
            newW = std::max(minW, mouseX - r.x);
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::BottomLeft: {
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            newH = std::max(minH, mouseY - r.y);
            break;
        }
        case ResizeDirection::BottomRight: {
            newW = std::max(minW, mouseX - r.x);
            newH = std::max(minH, mouseY - r.y);
            break;
        }
        default:
            break;
    }

    r.x = newX;
    r.y = newY;
    r.w = newW;
    r.h = newH;

    // Additional safety to keep title bar + buttons accessible above the taskbar
    const int desktopBottom = getUsableDesktopBottom();
    if (r.y < 0) r.y = 0;
    if (r.y + Window::TITLE_BAR_HEIGHT > desktopBottom) {
        r.y = std::max(0, desktopBottom - Window::TITLE_BAR_HEIGHT);
    }
    if (r.y + r.h > desktopBottom) {
        r.h = std::max(minH, desktopBottom - r.y);
    }
    if (r.x + r.w < 60) {
        r.x = 60 - r.w;
    }

    // Notify the app of the new client size (if any)
    if (window->app) {
        const int clientW = r.w;
        const int clientH = r.h - Window::TITLE_BAR_HEIGHT;
        window->app->onResize(clientW, clientH > 0 ? clientH : 0);
    }
}

void WindowManager::closeWindow(Window* window) {
    if (!window) return;

    // Let apps veto close once (dirty document guards). A second close after arming succeeds.
    if (window->app && !window->app->allowClose()) {
        return;
    }

    endAltTabCycle();

    // Capture before any mutation so we know which app type (if any) needs compaction
    // after this window is gone.
    std::string closedBase;
    if (!window->appBaseTitle.empty() && window->appInstanceNumber > 0) {
        closedBase = window->appBaseTitle;
    }

    // Unregister from file editor tracking if this window was responsible for a file
    if (!window->editedFilePath.empty()) {
        auto fedIt = m_fileEditors.find(window->editedFilePath);
        if (fedIt != m_fileEditors.end() && fedIt->second == window) {
            m_fileEditors.erase(fedIt);
        }
    }

    if (!window->drawingFilePath.empty()) {
        auto drawIt = m_fileDrawings.find(window->drawingFilePath);
        if (drawIt != m_fileDrawings.end() && drawIt->second == window) {
            m_fileDrawings.erase(drawIt);
        }
    }

    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        const bool wasFocused = (m_focusedWindow == window);

        if (m_focusedWindow == window) m_focusedWindow = nullptr;
        if (m_draggedWindow == window) m_draggedWindow = nullptr;
        if (m_mouseCaptureWindow == window) m_mouseCaptureWindow = nullptr;
        if (m_resizingWindow == window) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }

        // Notify before destruction so apps can clear focus-dependent state (e.g. Snake pause).
        if (wasFocused && window->app) {
            window->app->onFocusLost();
        }

        // Drop the app's controller pointer before destroying owned controller + app.
        if (window->app) {
            window->app->setController(nullptr);
        }
        window->controller.reset();

        // Clean up cached title texture for the window being closed
        auto cacheIt = m_titleCache.find(window->id);
        if (cacheIt != m_titleCache.end()) {
            if (cacheIt->second.texture) {
                SDL_DestroyTexture(cacheIt->second.texture);
            }
            m_titleCache.erase(cacheIt);
        }

        m_windows.erase(it);

        // Keep keyboard / client input working: promote the topmost non-minimized
        // survivor. Without this, m_focusedWindow stayed null until the next click.
        if (wasFocused && !m_windows.empty()) {
            // Raises z-order if needed and fires onFocusGained via bringToFront.
            focusTopmostVisibleWindow();
        }
    }

    // If this was a numbered instance of an app type, re-compact the remaining
    // live windows of the same type. This makes the numbers "adjust dynamically":
    // e.g. closing "Settings" will turn the former "Settings 2" into plain "Settings",
    // closing a middle one will shift higher numbers down, etc. Titles and the
    // active set are updated for the survivors; no gaps while windows are open.
    if (!closedBase.empty()) {
        compactAppInstances(closedBase);
    }
}

void WindowManager::clampWindowsToDesktop() {
    for (auto& winPtr : m_windows) {
        if (winPtr) {
            clampSingleWindow(*winPtr);
        }
    }
}

void WindowManager::notifyAppResizeIfGeometryChanged(Window& window,
                                                      const SDL_Rect& before) {
    if (!window.app
        || (window.rect.x == before.x
            && window.rect.y == before.y
            && window.rect.w == before.w
            && window.rect.h == before.h)) {
        return;
    }

    const int clientHeight = window.rect.h - Window::TITLE_BAR_HEIGHT;
    window.app->onResize(window.rect.w, clientHeight > 0 ? clientHeight : 0);
}

void WindowManager::clampSingleWindow(Window& w) {
    if (w.minimized) return;

    const int titleH = Window::TITLE_BAR_HEIGHT;
    const int minButtonSpace = kMinimumVisibleButtonSpace;

    SDL_Rect& r = w.rect;

    const SDL_Rect usable = getUsableDesktopRect();
    const int desktopBottom = usable.y + usable.h;
    const int usableHeight = std::max(titleH, usable.h);

    // Vertical - title bar fully visible above the taskbar
    if (r.y < 0) r.y = 0;
    if (r.y + titleH > desktopBottom) {
        r.y = desktopBottom - titleH;
    }
