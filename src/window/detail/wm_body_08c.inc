
bool WindowManager::saveSession(const std::string& hostPath) const {
    return monolith::detail::writeTextAtomically(
        hostPath,
        [this](std::ostream& out) {
            out << "session_v1\n";
            out << "# kind x y w h minimized maximized quoted-path\n";

            for (const auto& winPtr : m_windows) {
                if (!winPtr) continue;
                const Window& w = *winPtr;

                std::string kind;
                std::string path = "-";

                #include "wm_session_kind.inc"

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
        });
}

void WindowManager::requestQuit() {
    if (m_quitRequested) return;

    // Shut Down is still a close operation. Ask every app in this request so
    // multiple dirty documents are armed together instead of serializing them.
    // A callback can open another window, so continue with newly observed IDs
    // before accepting quit. IDs are monotonic, which also prevents rechecking
    // an original app after a callback changes the window set.
    bool allAllowed = true;
    std::set<int> checkedWindowIds;
    while (allAllowed) {
        bool checkedNewWindow = false;
        forEachLiveWindow([&](Window& window) {
            const int windowId = window.id;
            if (!checkedWindowIds.insert(windowId).second) return;
            checkedNewWindow = true;
            if (window.app && !window.app->allowClose()) allAllowed = false;
        });
        if (!checkedNewWindow) break;
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

void WindowManager::setStartMenuVisible(bool visible, bool resumeFocusedApp) {
    if (m_showStartMenu == visible) return;

    if (visible) {
        m_showStartMenu = true;
        m_startMenuFilter.clear();
        m_startMenuItems.clear();
        m_startMenuRect = {0, 0, 0, 0};
        m_startMenuHoverIndex = -1;
        m_startMenuKeyboardIndex = -1;
        clearStartMenuSuspension();
        if (m_hostFocused && m_focusedWindow && m_focusedWindow->app
            && !m_focusedWindow->minimized) {
            m_startMenuSuspendedWindow = m_focusedWindow;
            m_startMenuSuspendedWindowId = m_focusedWindow->id;
            invokeAppCallback([&]() { m_focusedWindow->app->onFocusLost(); });
        }
        return;
    }

    Window* resumedWindow = m_focusedWindow;
    const int resumedWindowId = resumedWindow ? resumedWindow->id : -1;
    m_showStartMenu = false;
    m_startMenuFilter.clear();
    if (resumeFocusedApp && m_hostFocused && isLiveWindow(resumedWindow, resumedWindowId)
        && resumedWindow->app && !resumedWindow->minimized) {
        clearStartMenuSuspension();
        // Resume whichever window owns logical focus now. The suspended app
        // may have closed or opened another window from onFocusLost().
        invokeAppCallback([&]() { resumedWindow->app->onFocusGained(); });
    } else if (resumeFocusedApp || !isStartMenuSuspended(m_startMenuSuspendedWindow)) {
        clearStartMenuSuspension();
    }
    m_startMenuKeyboardIndex = -1;
}

void WindowManager::invalidateStartMenuHitTargets() {
    m_startMenuItems.clear();
    m_startMenuRect = {0, 0, 0, 0};
    m_startMenuHoverIndex = -1;
}

void WindowManager::applyStartMenuFilterEdit() {
    invalidateStartMenuHitTargets();
    ensureStartMenuHitTargets();
    const int count = static_cast<int>(m_startMenuItems.size());
    if (count <= 0) {
        m_startMenuKeyboardIndex = -1;
    } else if (m_startMenuKeyboardIndex >= count) {
        m_startMenuKeyboardIndex = count - 1;
    }
}

#include "wm_body_08c_launch.inc"

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

    if (m_showStartMenu && event.type == SDL_TEXTINPUT) {
        const char* text = event.text.text;
        if (text && text[0] != '\0') {
            // Ignore control characters; printable type-ahead only.
            bool usable = false;
            for (const char* p = text; *p; ++p) {
                const unsigned char c = static_cast<unsigned char>(*p);
                if (c >= 32 && c != 127) {
                    usable = true;
                    break;
                }
            }
            if (usable) {
                m_startMenuFilter.append(text);
                applyStartMenuFilterEdit();
            }
        }
        return true;
    }

    if (event.type != SDL_KEYDOWN) return false;

    const SDL_Keysym& key = event.key.keysym;
    const bool alt = (key.mod & KMOD_ALT) != 0;
    const bool ctrl = (key.mod & KMOD_CTRL) != 0;

    if (alt && key.sym == SDLK_TAB) {
        // cycleFocus performs the actual focus handoff after the modal menu
        // closes, so let it consume the suspension marker.
        setStartMenuVisible(false, false);
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

    if (m_showStartMenu) {
        ensureStartMenuHitTargets();
        if (key.sym == SDLK_ESCAPE) {
            if (!m_startMenuFilter.empty()) {
                m_startMenuFilter.clear();
                applyStartMenuFilterEdit();
                // Keep the complete Escape gesture shell-owned even when it
                // only clears the filter and leaves the menu open.
                m_shellKeyUpsPending.insert(SDLK_ESCAPE);
                return true;
            }
            setStartMenuVisible(false);
            clearDesktopIconSelection();
            // Keep the complete Escape gesture shell-owned. Without a pending
            // release, the key-up can reach the newly resumed client after the
            // menu closes and trigger an unrelated prompt cancellation.
            m_shellKeyUpsPending.insert(SDLK_ESCAPE);
            return true;
        }

        if (key.sym == SDLK_BACKSPACE) {
            if (!m_startMenuFilter.empty()) {
                // Pop one UTF-8 code point (ASCII labels are the common case).
                std::size_t i = m_startMenuFilter.size() - 1;
                while (i > 0
                       && (static_cast<unsigned char>(m_startMenuFilter[i]) & 0xC0)
                              == 0x80) {
                    --i;
                }
                m_startMenuFilter.erase(i);
                applyStartMenuFilterEdit();
            }
            m_shellKeyUpsPending.insert(SDLK_BACKSPACE);
            return true;
        }

        if (key.sym == SDLK_UP || key.sym == SDLK_DOWN) {
            if (m_startMenuItems.empty()) return true;
            const int count = static_cast<int>(m_startMenuItems.size());
            if (m_startMenuKeyboardIndex < 0
                || m_startMenuKeyboardIndex >= count) {
                m_startMenuKeyboardIndex = key.sym == SDLK_UP ? count - 1 : 0;
            } else if (key.sym == SDLK_UP) {
                m_startMenuKeyboardIndex =
                    (m_startMenuKeyboardIndex + count - 1) % count;
            } else {
                m_startMenuKeyboardIndex =
                    (m_startMenuKeyboardIndex + 1) % count;
            }
            return true;
        }

        if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER) {
            if (m_startMenuItems.empty()) return true;
            if (m_startMenuKeyboardIndex < 0
                || m_startMenuKeyboardIndex >= static_cast<int>(m_startMenuItems.size())) {
                m_startMenuKeyboardIndex = 0;
            }
            // Activation closes the menu and may focus a newly launched app;
            // keep the matching release out of that app's input stream.
            m_shellKeyUpsPending.insert(key.sym);
            activateStartMenuItem(
                m_startMenuItems[static_cast<size_t>(m_startMenuKeyboardIndex)].action);
            return true;
        }
    }

    return false;
}

bool WindowManager::shouldQuit() const {
    return m_quitRequested;
}
