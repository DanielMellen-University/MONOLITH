#include "WindowManager.hpp"
#include "../app/App.hpp"
#include "../app/TerminalApp.hpp"
#include "../app/TextEditorApp.hpp"
#include "../app/FilesystemApp.hpp"
#include "../app/SettingsApp.hpp"
#include <algorithm>

namespace monolith::window {

namespace {
constexpr int kTaskbarHeight = 28;
}

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() = default;

Window* WindowManager::createWindow(const std::string& title, int x, int y, int w, int h,
                                        std::unique_ptr<monolith::app::App> app) {
    auto window = std::make_unique<Window>();
    window->id = m_nextId++;
    window->title = title;
    window->rect.x = x;
    window->rect.y = y;
    window->rect.w = w;
    window->rect.h = h;
    window->app = std::move(app);

    Window* ptr = window.get();
    m_windows.push_back(std::move(window));

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
    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;

        // Forward motion to the focused window's app if the pointer is in its content area
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized) {
            const int logicalY = screenToLogicalY(m_mouseY);
            if (isInContentArea(*m_focusedWindow, m_mouseX, logicalY)) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*m_focusedWindow, clientEvent);
                m_focusedWindow->app->handleEvent(clientEvent);
            }
        }
    }

    const int logicalMouseY = screenToLogicalY(m_mouseY);

    // Forward keyboard events (text input, key down/up) to the focused window's app
    if (m_focusedWindow && m_focusedWindow->app &&
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT)) {
        m_focusedWindow->app->handleEvent(event);
        // Do not return — WM doesn't consume keyboard yet, but apps get first crack
    }

    // === Global UI first: Taskbar + Start Menu (must be checked even if click misses all windows) ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        const int taskbarHeight = kTaskbarHeight;
        const int taskbarScreenY = logicalToScreenY(m_logicalHeight - taskbarHeight);
        const int taskbarScreenBottom = logicalToScreenY(m_logicalHeight);

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
                        case 4: requestQuit(); break;
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

    // === Taskbar scroll arrows (Windows XP style) ===
    if (m_taskbarNeedsScroll && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int taskbarScreenY = logicalToScreenY(m_logicalHeight - kTaskbarHeight);
        const int taskbarScreenBottom = logicalToScreenY(m_logicalHeight);

        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
            int arrowW = 16;
            int leftArrowX = logicalToScreenX(m_taskbarButtonAreaLeft);
            int rightArrowX = logicalToScreenX(m_taskbarButtonAreaLeft + m_taskbarButtonAreaWidth + arrowW + 4);

            if (m_mouseX >= leftArrowX && m_mouseX < leftArrowX + static_cast<int>(arrowW * m_contentScale)) {
                m_taskbarScrollOffset = std::max(0, m_taskbarScrollOffset - 120);
                return;
            }
            if (m_mouseX >= rightArrowX && m_mouseX < rightArrowX + static_cast<int>(arrowW * m_contentScale)) {
                // We don't know exact max here, but we can allow generous scrolling; it will be limited by drawing logic
                m_taskbarScrollOffset += 120;
                return;
            }
        }
    }

    // Mouse wheel over taskbar scrolls the window buttons
    if (event.type == SDL_MOUSEWHEEL) {
        const int taskbarScreenY = logicalToScreenY(m_logicalHeight - kTaskbarHeight);
        const int taskbarScreenBottom = logicalToScreenY(m_logicalHeight);
        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom && m_taskbarNeedsScroll) {
            m_taskbarScrollOffset += (event.wheel.y > 0 ? -80 : 80);
            if (m_taskbarScrollOffset < 0) m_taskbarScrollOffset = 0;
            // Upper bound will be soft-limited by drawing (buttons just stop appearing)
            return;
        }
    }

    // === Normal internal window interaction ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (!clicked) return;

        bringToFront(clicked);

        // === Priority 1: Title bar buttons ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            if (handleTitleBarButtons(clicked, m_mouseX, logicalMouseY)) {
                return;
            }
        }

        // === Priority 2: Resize borders ===
        ResizeDirection dir = getResizeDirection(*clicked, m_mouseX, logicalMouseY);
        if (dir != ResizeDirection::None && !clicked->maximized) {
            m_resizingWindow = clicked;
            m_resizeDirection = dir;
            return;
        }

        // === Priority 3: Title bar drag ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            m_draggedWindow = clicked;
            clicked->beingDragged = true;
            clicked->dragOffsetX = m_mouseX - clicked->rect.x;
            clicked->dragOffsetY = logicalMouseY - clicked->rect.y;
            return;
        }

        // === Priority 4: Client area of the window ===
        // Forward to the app (if any), with coordinates translated to the app's local space.
        if (isInContentArea(*clicked, m_mouseX, logicalMouseY)) {
            if (clicked->app) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*clicked, clientEvent);
                clicked->app->handleEvent(clientEvent);
            }
            return;
        }
    }

    // === Non-left mouse buttons (right-click for context menus, etc.) ===
    // These still need to reach the app's client area, but skip all the drag/resize/titlebar logic.
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button != SDL_BUTTON_LEFT) {
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (clicked) {
            bringToFront(clicked);

            if (isInContentArea(*clicked, m_mouseX, logicalMouseY)) {
                if (clicked->app) {
                    SDL_Event clientEvent = event;
                    translateMouseEventToClient(*clicked, clientEvent);
                    clicked->app->handleEvent(clientEvent);
                }
                return;
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = false;

        if (m_draggedWindow) {
            m_draggedWindow->beingDragged = false;
            m_draggedWindow = nullptr;
        }

        if (m_resizingWindow) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }
    }
}

void WindowManager::update() {
    const int logicalMouseY = screenToLogicalY(m_mouseY);

    if (m_draggedWindow && m_mouseDown) {
        m_draggedWindow->rect.x = m_mouseX - m_draggedWindow->dragOffsetX;
        m_draggedWindow->rect.y = logicalMouseY - m_draggedWindow->dragOffsetY;

        // Prevent user from dragging the window so far that buttons become inaccessible
        clampSingleWindow(*m_draggedWindow);
    }

    if (m_resizingWindow && m_mouseDown) {
        applyResize(m_resizingWindow, m_mouseX, logicalMouseY);
        clampSingleWindow(*m_resizingWindow);
    }

    // Enforce maximized windows fill the full logical area (minus the always-visible taskbar)
    // The taskbar is permanently at the bottom, so all maximized windows must
    // always reserve space for it. This is a general rule for the entire desktop shell,
    // not something individual apps should have to handle.
    for (auto& w : m_windows) {
        if (w->maximized && !w->minimized) {
            w->rect.x = 0;
            w->rect.y = 0;
            w->rect.w = m_logicalWidth;
            w->rect.h = m_logicalHeight - kTaskbarHeight;   // always reserve for the taskbar
        }
    }
}

void WindowManager::render(SDL_Renderer* renderer) {
    // Draw from back to front so the focused window (last in vector) is drawn last and appears on top
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        Window& win = **it;

        if (win.minimized) continue;

        // Convert logical window rect to screen coordinates (shifted down by GNOME header + scaled)
        const int screenX = logicalToScreenX(win.rect.x);
        const int screenY = logicalToScreenY(win.rect.y);

        const int scaledW = static_cast<int>(win.rect.w * m_contentScale);
        const int scaledH = static_cast<int>(win.rect.h * m_contentScale);
        const int scaledTitleH = static_cast<int>(Window::TITLE_BAR_HEIGHT * m_contentScale);

        // === Window background (content area) ===
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
        SDL_Rect contentRect = {screenX, screenY + scaledTitleH, scaledW, scaledH - scaledTitleH};
        SDL_RenderFillRect(renderer, &contentRect);

        // Delegate to the app (if present) so it can draw its own content
        if (win.app) {
            win.app->render(renderer, contentRect);
        }

        // === Title bar ===
        bool isFocused = (&win == m_focusedWindow);
        SDL_Color titleColor = isFocused ? SDL_Color{60, 60, 70, 255}
                                         : SDL_Color{40, 40, 48, 255};

        SDL_SetRenderDrawColor(renderer, titleColor.r, titleColor.g, titleColor.b, 255);
        SDL_Rect titleBar = {screenX, screenY, scaledW, scaledTitleH};
        SDL_RenderFillRect(renderer, &titleBar);

        // === Window title text (cached) ===
        if (m_font && !win.title.empty()) {
            auto& cache = m_titleCache[win.id];
            bool needsUpdate = (cache.texture == nullptr) ||
                               (cache.lastTitle != win.title) ||
                               (cache.wasFocused != isFocused);

            if (needsUpdate) {
                if (cache.texture) {
                    SDL_DestroyTexture(cache.texture);
                }

                SDL_Color textColor = isFocused ? SDL_Color{230, 230, 235, 255}
                                                : SDL_Color{180, 180, 185, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(m_font, win.title.c_str(), textColor);
                if (surface) {
                    cache.texture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_FreeSurface(surface);
                }
                cache.lastTitle = win.title;
                cache.wasFocused = isFocused;
            }

            if (cache.texture) {
                int texW, texH;
                SDL_QueryTexture(cache.texture, nullptr, nullptr, &texW, &texH);

                int paddingLeft = 10;
                int availableWidth = scaledW - 80;

                SDL_Rect dstRect = {
                    screenX + paddingLeft,
                    screenY + (scaledTitleH - texH) / 2,
                    texW,
                    texH
                };

                if (texW > availableWidth) {
                    dstRect.w = availableWidth;
                }

                SDL_RenderCopy(renderer, cache.texture, nullptr, &dstRect);
            }
        }

        // === Window buttons (close, minimize, maximize) with simple symbols ===
        int buttonSize = static_cast<int>(16 * m_contentScale);
        if (buttonSize < 10) buttonSize = 10; // minimum usable size
        int buttonY = screenY + (scaledTitleH - buttonSize) / 2;
        int buttonSpacing = static_cast<int>(6 * m_contentScale);
        int rightEdge = screenX + scaledW - static_cast<int>(10 * m_contentScale);

        // Close button (red) - draw white X
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);
        SDL_Rect closeBtn = {rightEdge - buttonSize, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &closeBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, closeBtn.x + 3, closeBtn.y + 3, closeBtn.x + buttonSize - 4, closeBtn.y + buttonSize - 4);
        SDL_RenderDrawLine(renderer, closeBtn.x + buttonSize - 4, closeBtn.y + 3, closeBtn.x + 3, closeBtn.y + buttonSize - 4);

        // Maximize (green) - draw white square outline
        SDL_SetRenderDrawColor(renderer, 60, 170, 80, 255);
        SDL_Rect maxBtn = {rightEdge - buttonSize * 2 - buttonSpacing, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &maxBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect maxSymbol = {maxBtn.x + 3, maxBtn.y + 3, buttonSize - 7, buttonSize - 7};
        SDL_RenderDrawRect(renderer, &maxSymbol);

        // Minimize (yellow/orange) - draw white horizontal line
        SDL_SetRenderDrawColor(renderer, 200, 170, 60, 255);
        SDL_Rect minBtn = {rightEdge - buttonSize * 3 - buttonSpacing * 2, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &minBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int minLineY = minBtn.y + buttonSize / 2;
        SDL_RenderDrawLine(renderer, minBtn.x + 3, minLineY, minBtn.x + buttonSize - 4, minLineY);

        // === Window border (drawn in screen coordinates) ===
        SDL_Rect screenRect = {screenX, screenY, scaledW, scaledH};
        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderDrawRect(renderer, &screenRect);
    }

    // === Start Menu popup ===
    m_startMenuItems.clear();
    m_startMenuHoverIndex = -1;
    m_startMenuRect = {0, 0, 0, 0};

    if (m_showStartMenu) {
        const int menuWidth = 210;
        const int menuHeight = 260;
        const int menuX = logicalToScreenX(8);
        const int menuY = logicalToScreenY(m_logicalHeight - kTaskbarHeight - menuHeight);

        // Menu background (slightly more XP-like dark panel)
        SDL_SetRenderDrawColor(renderer, 38, 38, 48, 255);
        SDL_Rect menuRect = {
            menuX, menuY,
            static_cast<int>(menuWidth * m_contentScale),
            static_cast<int>(menuHeight * m_contentScale)
        };
        m_startMenuRect = menuRect;   // save for click hit-testing (popup is above taskbar)
        SDL_RenderFillRect(renderer, &menuRect);
        SDL_SetRenderDrawColor(renderer, 85, 85, 95, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Top accent bar (subtle header area)
        SDL_SetRenderDrawColor(renderer, 55, 85, 140, 255);
        SDL_Rect headerAccent = {
            menuX, menuY,
            static_cast<int>(menuWidth * m_contentScale),
            static_cast<int>(22 * m_contentScale)
        };
        SDL_RenderFillRect(renderer, &headerAccent);

        if (m_font) {
            // Small "Monolith" label in the accent area
            SDL_Color accentText = {255, 255, 255, 255};
            SDL_Surface* hdr = TTF_RenderText_Blended(m_font, "Monolith", accentText);
            if (hdr) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hdr);
                if (ht) {
                    SDL_Rect dst = {
                        menuX + static_cast<int>(10 * m_contentScale),
                        menuY + static_cast<int>(4 * m_contentScale),
                        hdr->w, hdr->h
                    };
                    SDL_RenderCopy(renderer, ht, nullptr, &dst);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hdr);
            }
        }

        if (m_font) {
            SDL_Color textCol = {235, 235, 240, 255};
            SDL_Color hoverTextCol = {255, 255, 255, 255};
            SDL_Color hoverBgCol = {70, 95, 145, 255};

            struct MenuEntry { const char* label; int action; };
            MenuEntry entries[] = {
                {"Terminal", 0},
                {"Text Editor", 1},
                {"Filesystem", 2},
                {"Settings", 3},
                {"Shut Down", 4}
            };

            // Start items below the accent header
            int y = menuY + static_cast<int>(28 * m_contentScale);
            const int itemH = static_cast<int>(26 * m_contentScale);
            const int itemPad = static_cast<int>(10 * m_contentScale);

            for (size_t i = 0; i < sizeof(entries)/sizeof(entries[0]); ++i) {
                const auto& e = entries[i];

                // Clickable rect (in screen space)
                SDL_Rect itemRect = {
                    menuX + 4,
                    y,
                    static_cast<int>((menuWidth - 8) * m_contentScale),
                    itemH
                };
                m_startMenuItems.push_back({itemRect, e.action});

                // Hover test using latest known mouse position
                bool hovered = false;
                SDL_Point mousePt = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&mousePt, &itemRect)) {
                    hovered = true;
                    m_startMenuHoverIndex = static_cast<int>(i);
                }

                // Draw hover highlight if needed
                if (hovered) {
                    SDL_SetRenderDrawColor(renderer, hoverBgCol.r, hoverBgCol.g, hoverBgCol.b, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                }

                SDL_Color useTextCol = hovered ? hoverTextCol : textCol;

                SDL_Surface* s = TTF_RenderText_Blended(m_font, e.label, useTextCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {menuX + itemPad, y + (itemH - s->h)/2, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }

                y += itemH + static_cast<int>(2 * m_contentScale);
            }
        }
    }

    // === Taskbar (always visible) ===
    m_taskbarEntries.clear();

    const int taskbarHeight = kTaskbarHeight;
    const int taskbarY = m_logicalHeight - taskbarHeight;

    // Draw taskbar background
    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 255);
    SDL_Rect taskbarRect = {
        logicalToScreenX(0),
        logicalToScreenY(taskbarY),
        static_cast<int>(m_logicalWidth * m_contentScale),
        static_cast<int>(taskbarHeight * m_contentScale)
    };
    SDL_RenderFillRect(renderer, &taskbarRect);

    // --- Left: Start button ---
    int x = logicalToScreenX(8);
    const int startBtnWidth = static_cast<int>(70 * m_contentScale);
    const int btnHeight = static_cast<int>(22 * m_contentScale);
    const int btnY = logicalToScreenY(taskbarY) + (taskbarRect.h - btnHeight) / 2;

    SDL_SetRenderDrawColor(renderer, 55, 85, 140, 255);
    SDL_Rect startBtn = {x, btnY, startBtnWidth, btnHeight};
    SDL_RenderFillRect(renderer, &startBtn);

    if (m_font) {
        SDL_Color textCol = {255, 255, 255, 255};
        SDL_Surface* s = TTF_RenderText_Blended(m_font, "Start", textCol);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect dst = {x + 8, btnY + (btnHeight - s->h)/2, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Compute scrollable button area (leave room for clock on the right)
    const int clockReserve = 90; // logical pixels reserved for clock
    m_taskbarButtonAreaLeft = 8 + 70 + 8;  // after Start button + padding (logical)
    m_taskbarButtonAreaWidth = m_logicalWidth - m_taskbarButtonAreaLeft - clockReserve - 8;

    // Calculate total width + prepare drawing order (active/focused window always leftmost)
    std::vector<Window*> buttonOrder;
    if (m_focusedWindow) {
        buttonOrder.push_back(m_focusedWindow);
    }
    for (auto& w : m_windows) {
        if (w.get() != m_focusedWindow) {
            buttonOrder.push_back(w.get());
        }
    }

    int totalButtonsWidth = 0;
    std::vector<int> buttonWidths;
    for (Window* win : buttonOrder) {
        int bw = std::max(85, static_cast<int>(win->title.length() * 6.5f));
        buttonWidths.push_back(bw);
        totalButtonsWidth += bw + 6;
    }

    m_taskbarNeedsScroll = (totalButtonsWidth > m_taskbarButtonAreaWidth);

    // Draw scroll arrows if needed
    int arrowWidth = 16;
    int buttonsStartX = m_taskbarButtonAreaLeft;

    if (m_taskbarNeedsScroll) {
        // Left arrow
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_Rect leftArrow = {
            logicalToScreenX(m_taskbarButtonAreaLeft),
            btnY,
            static_cast<int>(arrowWidth * m_contentScale),
            btnHeight
        };
        SDL_RenderFillRect(renderer, &leftArrow);
        // Simple left triangle using lines
        SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
        int ax = logicalToScreenX(m_taskbarButtonAreaLeft + 4);
        int ay = btnY + btnHeight/2;
        SDL_RenderDrawLine(renderer, ax + 6, ay - 5, ax, ay);
        SDL_RenderDrawLine(renderer, ax, ay, ax + 6, ay + 5);

        buttonsStartX += arrowWidth + 4;

        // Right arrow
        int rightX = m_taskbarButtonAreaLeft + m_taskbarButtonAreaWidth - arrowWidth;
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_Rect rightArrow = {
            logicalToScreenX(rightX),
            btnY,
            static_cast<int>(arrowWidth * m_contentScale),
            btnHeight
        };
        SDL_RenderFillRect(renderer, &rightArrow);
        SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
        ax = logicalToScreenX(rightX + 4);
        SDL_RenderDrawLine(renderer, ax, ay - 5, ax + 6, ay);
        SDL_RenderDrawLine(renderer, ax + 6, ay, ax, ay + 5);

        m_taskbarButtonAreaWidth -= (arrowWidth + 4) * 2;
    }

    // --- Middle: Window buttons (ALL windows, XP style) ---
    // Active window is always drawn leftmost (per user request)
    int currentX = logicalToScreenX(buttonsStartX) - static_cast<int>(m_taskbarScrollOffset * m_contentScale);

    for (size_t i = 0; i < buttonOrder.size(); ++i) {
        Window* win = buttonOrder[i];
        int bw = buttonWidths[i];

        SDL_Rect btnRect = {
            currentX,
            btnY,
            static_cast<int>(bw * m_contentScale),
            btnHeight
        };

        // Only consider for hit testing if it would be at least partially visible in the button area
        int areaLeftScreen = logicalToScreenX(buttonsStartX);
        int areaRightScreen = logicalToScreenX(buttonsStartX + m_taskbarButtonAreaWidth);

        bool visible = (btnRect.x + btnRect.w > areaLeftScreen) && (btnRect.x < areaRightScreen);

        // Determine button appearance (Windows XP-ish)
        bool isFocused = (win == m_focusedWindow && !win->minimized);
        bool isMin = win->minimized;

        Uint8 r = 65, g = 65, b = 72;
        if (isFocused) {
            r = 85; g = 95; b = 130;   // highlighted for active
        } else if (isMin) {
            r = 55; g = 55; b = 62;    // darker for minimized
        }

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderFillRect(renderer, &btnRect);

        // Subtle border
        SDL_SetRenderDrawColor(renderer, 100, 100, 108, 255);
        SDL_RenderDrawRect(renderer, &btnRect);

        // Title text (clipped)
        if (m_font) {
            SDL_Color textCol = isMin ? SDL_Color{170,170,175,255} : SDL_Color{235,235,240,255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, win->title.c_str(), textCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    int textW = std::min(s->w, btnRect.w - 10);
                    SDL_Rect textDst = {
                        btnRect.x + 5,
                        btnY + (btnHeight - s->h) / 2,
                        textW, s->h
                    };
                    SDL_RenderCopy(renderer, t, nullptr, &textDst);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        if (visible) {
            // Store for click handling (use the drawn rect)
            m_taskbarEntries.push_back({btnRect, win});
        }

        currentX += static_cast<int>((bw + 6) * m_contentScale);
    }

    // --- Right: System clock ---
    {
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%I:%M %p", tm_info);

        if (m_font) {
            SDL_Color textCol = {225, 225, 230, 255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, timeStr, textCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    int clockX = logicalToScreenX(m_logicalWidth) - s->w - static_cast<int>(10 * m_contentScale);
                    int clockY = btnY + (btnHeight - s->h) / 2;
                    SDL_Rect dst = {clockX, clockY, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

Window* WindowManager::getWindowAt(int mouseX, int mouseY) {
    const int logicalMouseY = screenToLogicalY(mouseY);

    // Check from front to back (focused window first)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (!win.minimized) {
            SDL_Rect logicalRect = win.rect;
            SDL_Point p = {mouseX, logicalMouseY};
            if (SDL_PointInRect(&p, &logicalRect)) {
                return &win;
            }
        }
    }
    return nullptr;
}

void WindowManager::bringToFront(Window* window) {
    if (!window) return;

    // Move the window to the end of the vector (drawn last = on top)
    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        auto windowPtr = std::move(*it);
        m_windows.erase(it);
        m_windows.push_back(std::move(windowPtr));

        Window* newFocused = m_windows.back().get();

        // Notify focus change
        if (m_focusedWindow && m_focusedWindow != newFocused && m_focusedWindow->app) {
            m_focusedWindow->app->onFocusLost();
        }
        if (newFocused && newFocused->app && m_focusedWindow != newFocused) {
            newFocused->app->onFocusGained();
        }

        m_focusedWindow = newFocused;

        // When active window changes, reset taskbar scroll so the new leftmost (active) button is visible
        m_taskbarScrollOffset = 0;
    }
}

bool WindowManager::isInTitleBar(const Window& window, int x, int y) const {
    SDL_Rect titleBar = {
        window.rect.x,
        window.rect.y,
        window.rect.w,
        Window::TITLE_BAR_HEIGHT
    };
    SDL_Point p = {x, y};
    return SDL_PointInRect(&p, &titleBar);
}

void WindowManager::setFont(TTF_Font* font) {
    if (m_font != font) {
        // Invalidate all title caches when font changes
        for (auto& [id, entry] : m_titleCache) {
            if (entry.texture) {
                SDL_DestroyTexture(entry.texture);
            }
        }
        m_titleCache.clear();
    }
    m_font = font;
}

void WindowManager::setLogicalDesktopSize(int width, int height) {
    m_logicalWidth = width;
    m_logicalHeight = height;

    // Re-clamp when logical size changes
    clampWindowsToDesktop();
}

void WindowManager::setHeaderOffset(int offsetY) {
    m_headerOffset = offsetY;
}

void WindowManager::setContentScale(float scale) {
    m_contentScale = scale;
}

ResizeDirection WindowManager::getResizeDirectionAt(int mouseX, int mouseY) const {
    // Check from front to back
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (win.minimized || win.maximized) continue;

        SDL_Point p = {mouseX, mouseY};
        if (SDL_PointInRect(&p, &win.rect)) {
            return getResizeDirection(win, mouseX, mouseY);
        }
    }
    return ResizeDirection::None;
}

bool WindowManager::handleTitleBarButtons(Window* window, int mouseX, int mouseY) {
    if (!window) return false;

    const int buttonSize = 16;
    const int buttonSpacing = 6;
    const int rightEdge = window->rect.x + window->rect.w - 10;
    const int buttonY = window->rect.y + (Window::TITLE_BAR_HEIGHT - buttonSize) / 2;

    SDL_Rect closeBtn   = {rightEdge - buttonSize,                           buttonY, buttonSize, buttonSize};
    SDL_Rect maxBtn     = {rightEdge - buttonSize * 2 - buttonSpacing,       buttonY, buttonSize, buttonSize};
    SDL_Rect minBtn     = {rightEdge - buttonSize * 3 - buttonSpacing * 2,   buttonY, buttonSize, buttonSize};

    SDL_Point mouse = {mouseX, mouseY};

    if (SDL_PointInRect(&mouse, &closeBtn)) {
        closeWindow(window);
        return true;
    }

    if (SDL_PointInRect(&mouse, &maxBtn)) {
        if (window->maximized) {
            // Restore previous size/position
            window->rect = window->previousRect;
            window->maximized = false;
        } else {
            // Maximize to fill the logical desktop, leaving space for the taskbar at the bottom.
            // (the title bar sits at the top of the logical area, content fills the rest)
            window->previousRect = window->rect;

            window->rect.x = 0;
            window->rect.y = 0;
            window->rect.w = m_logicalWidth;
            window->rect.h = m_logicalHeight - kTaskbarHeight;   // reserve space for the always-visible taskbar

            window->maximized = true;
        }

        // Notify app of size change
        if (window->app) {
            const int clientH = window->rect.h - Window::TITLE_BAR_HEIGHT;
            window->app->onResize(window->rect.w, clientH > 0 ? clientH : 0);
        }
        return true;
    }

    if (SDL_PointInRect(&mouse, &minBtn)) {
        window->minimized = true;
        return true;
    }

    return false;
}

ResizeDirection WindowManager::getResizeDirection(const Window& window, int mouseX, int mouseY) const {
    const int border = 8; // Resize border thickness (slightly generous for usability)

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
    const int desktopBottom = m_logicalHeight - kTaskbarHeight;
    if (r.y < 0) r.y = 0;
    if (r.y + Window::TITLE_BAR_HEIGHT > desktopBottom) {
        r.y = desktopBottom - Window::TITLE_BAR_HEIGHT;
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

    // Unregister from file editor tracking if this window was responsible for a file
    if (!window->editedFilePath.empty()) {
        auto fedIt = m_fileEditors.find(window->editedFilePath);
        if (fedIt != m_fileEditors.end() && fedIt->second == window) {
            m_fileEditors.erase(fedIt);
        }
    }

    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        if (m_focusedWindow == window) m_focusedWindow = nullptr;
        if (m_draggedWindow == window) m_draggedWindow = nullptr;
        if (m_resizingWindow == window) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }

        // Clean up cached title texture
        auto cacheIt = m_titleCache.find(window->id);
        if (cacheIt != m_titleCache.end()) {
            if (cacheIt->second.texture) {
                SDL_DestroyTexture(cacheIt->second.texture);
            }
            m_titleCache.erase(cacheIt);
        }

        m_windows.erase(it);
    }
}

void WindowManager::clampWindowsToDesktop() {
    for (auto& winPtr : m_windows) {
        if (winPtr) {
            clampSingleWindow(*winPtr);
        }
    }
}

void WindowManager::clampSingleWindow(Window& w) {
    if (w.minimized) return;

    const int titleH = Window::TITLE_BAR_HEIGHT;
    const int minButtonSpace = 80;

    SDL_Rect& r = w.rect;

    const int desktopBottom = m_logicalHeight - kTaskbarHeight;
    const int usableHeight = std::max(titleH, desktopBottom);

    // Vertical - title bar fully visible above the taskbar
    if (r.y < 0) r.y = 0;
    if (r.y + titleH > desktopBottom) {
        r.y = desktopBottom - titleH;
    }

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
        event.motion.x -= clientOriginX;
        event.motion.y -= clientOriginY;
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        event.button.x -= clientOriginX;
        event.button.y -= clientOriginY;
    } else if (event.type == SDL_MOUSEWHEEL) {
        // Wheel events don't have x/y in the same way; apps can use mouse position separately if needed
    }
}

// === Desktop shell launchers (used by Start Menu) ===

void WindowManager::setAppResources(TTF_Font* font, monolith::fs::Filesystem* fs) {
    m_appFont = font;
    m_fs = fs;
}

void WindowManager::launchTerminal() {
    if (!m_appFont) return;

    std::string title = "Terminal";
    // Give multiple instances a simple distinguishing suffix
    if (!m_windows.empty()) {
        title += " " + std::to_string(m_windows.size() + 1);
    }

    auto app = std::make_unique<monolith::app::TerminalApp>(m_appFont, m_fs);
    int x = 100 + (static_cast<int>(m_windows.size()) % 7) * 35;
    int y = 80 + (static_cast<int>(m_windows.size()) % 5) * 25;
    createWindow(title, x, y, 520, 380, std::move(app));
    m_showStartMenu = false;
}

void WindowManager::launchTextEditor(const std::string& initialPath) {
    if (!m_appFont) return;

    // Singleton behavior: if we already have an editor open for this exact file, just focus it.
    if (!initialPath.empty() && m_fs) {
        std::string normalized = m_fs->normalize(initialPath);
        auto it = m_fileEditors.find(normalized);
        if (it != m_fileEditors.end() && it->second) {
            bringToFront(it->second);
            m_showStartMenu = false;
            return;
        }
    }

    auto app = std::make_unique<monolith::app::TextEditorApp>(m_appFont, m_fs, initialPath);

    std::string title = "Editor";
    std::string baseName;
    if (!initialPath.empty()) {
        size_t slash = initialPath.find_last_of('/');
        baseName = (slash != std::string::npos) ? initialPath.substr(slash + 1) : initialPath;
        if (!baseName.empty()) title = "Editor - " + baseName;
    } else if (!m_windows.empty()) {
        title += " " + std::to_string(m_windows.size() + 1);
    }

    int x = 160 + (static_cast<int>(m_windows.size()) % 6) * 30;
    int y = 90 + (static_cast<int>(m_windows.size()) % 4) * 20;
    Window* w = createWindow(title, x, y, 540, 420, std::move(app));

    // Register for singleton tracking if this editor is bound to a specific file
    if (w && !initialPath.empty() && m_fs) {
        associateEditorWithFile(w, initialPath);
    }

    m_showStartMenu = false;
}

void WindowManager::launchFilesystem() {
    if (!m_appFont) return;

    auto app = std::make_unique<monolith::app::FilesystemApp>(m_appFont, m_fs);

    std::string title = "Filesystem";
    if (!m_windows.empty()) {
        title += " " + std::to_string(m_windows.size() + 1);
    }

    int x = 200 + (static_cast<int>(m_windows.size()) % 5) * 25;
    int y = 130 + (static_cast<int>(m_windows.size()) % 3) * 18;
    createWindow(title, x, y, 480, 360, std::move(app));
    m_showStartMenu = false;
}

void WindowManager::launchSettings() {
    if (!m_appFont) return;

    auto app = std::make_unique<monolith::app::SettingsApp>(m_appFont, m_fs);

    // Position it a bit more to the right/center-ish
    int x = 180 + (static_cast<int>(m_windows.size()) % 4) * 20;
    int y = 110 + (static_cast<int>(m_windows.size()) % 3) * 15;
    createWindow("Settings", x, y, 460, 320, std::move(app));
    m_showStartMenu = false;
}

void WindowManager::requestQuit() {
    m_quitRequested = true;
    m_showStartMenu = false;
}

bool WindowManager::shouldQuit() const {
    return m_quitRequested;
}

void WindowManager::associateEditorWithFile(Window* window, const std::string& virtualPath) {
    if (!window || virtualPath.empty() || !m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);

    // If something else was already registered for this path, clear the old association
    auto existing = m_fileEditors.find(normalized);
    if (existing != m_fileEditors.end() && existing->second && existing->second != window) {
        existing->second->editedFilePath.clear();
    }

    window->editedFilePath = normalized;
    m_fileEditors[normalized] = window;
}

// =============================================================================
// Per-window controller for apps (IWindowController implementation)
// =============================================================================

namespace {

// Small concrete controller that lets an App affect its host window.
struct WindowController : public monolith::app::IWindowController {
    monolith::window::WindowManager* wm;
    monolith::window::Window* targetWindow;

    void close() override {
        if (wm && targetWindow) wm->closeWindow(targetWindow);
    }

    void setTitle(const std::string& title) override {
        if (targetWindow) {
            targetWindow->title = title;
        }
    }

    void openInTextEditor(const std::string& virtualPath) override {
        if (wm) {
            wm->launchTextEditor(virtualPath);
        }
    }
};

} // anonymous namespace

monolith::app::IWindowController* WindowManager::createControllerFor(Window* window) {
    // We allocate a small controller per window that has an app.
    // Lifetime is tied to the app (destroyed when the window/app is closed).
    // For simplicity we leak them for now (or we could store them in a map).
    // A better long-term solution is to store them alongside the window.
    static std::vector<std::unique_ptr<WindowController>> s_controllers;

    auto ctrl = std::make_unique<WindowController>();
    ctrl->wm = this;
    ctrl->targetWindow = window;

    monolith::app::IWindowController* raw = ctrl.get();
    s_controllers.push_back(std::move(ctrl));
    return raw;
}

} // namespace monolith::window
