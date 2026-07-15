#include "WindowManager.hpp"
#include "../app/App.hpp"
#include "../app/TerminalApp.hpp"
#include "../app/TextEditorApp.hpp"
#include "../app/FilesystemApp.hpp"
#include "../app/SettingsApp.hpp"
#include "../app/DrawingApp.hpp"
#include "../app/SnakeApp.hpp"
#include "../app/MinesweeperApp.hpp"
#include <algorithm>

namespace monolith::window {

namespace {
constexpr int kTaskbarHeight = 28;
constexpr int kWindowButtonSize = 16;
constexpr int kWindowButtonSpacing = 6;
constexpr int kWindowButtonRightPadding = 10;
constexpr int kMinimumVisibleButtonSpace = 80;
}

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() = default;

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
    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;

        // Forward motion to the focused window's app if the pointer is in its content area.
        // Do NOT forward while the Start menu is open (the popup owns input in its area
        // and we don't want buried apps to see hovers/motion under the menu).
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized && !m_showStartMenu) {
            const int logicalY = screenToLogicalY(m_mouseY);
            if (isInContentArea(*m_focusedWindow, m_mouseX, logicalY)) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*m_focusedWindow, clientEvent);
                m_focusedWindow->app->handleEvent(clientEvent);
            }
        }
    }

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
                        case 7: requestQuit(); break;
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
        const SDL_Rect taskbarScreenRect = logicalRectToScreen(getTaskbarRect());
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;

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
        const SDL_Rect taskbarScreenRect = logicalRectToScreen(getTaskbarRect());
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;
        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom && m_taskbarNeedsScroll) {
            m_taskbarScrollOffset += (event.wheel.y > 0 ? -80 : 80);
            if (m_taskbarScrollOffset < 0) m_taskbarScrollOffset = 0;
            // Upper bound will be soft-limited by drawing (buttons just stop appearing)
            return;
        }

        // Forward wheel events to the focused app's client area (Settings scroll, etc.).
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized && !m_showStartMenu) {
            if (isInContentArea(*m_focusedWindow, m_mouseX, logicalMouseY)) {
                m_focusedWindow->app->handleEvent(event);
                return;
            }
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

        // Forward mouse-up to the focused app (needed for drawing and other drag-release flows).
        // Always deliver to the focused window so releases outside the client rect still end drags.
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized && !m_showStartMenu) {
            SDL_Event clientEvent = event;
            translateMouseEventToClient(*m_focusedWindow, clientEvent);
            m_focusedWindow->app->handleEvent(clientEvent);
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
            SDL_Rect usable = getUsableDesktopRect();
            w->rect = usable;
        }
    }

    // Per-frame app updates (games ticks, timers, etc.). Skip minimized windows.
    for (auto& w : m_windows) {
        if (w && w->app && !w->minimized) {
            w->app->update();
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
        const TitleButtonRects logicalButtons = getTitleButtonRects(win);
        SDL_Rect closeBtn = logicalRectToScreen(logicalButtons.close);
        SDL_Rect maxBtn = logicalRectToScreen(logicalButtons.maximize);
        SDL_Rect minBtn = logicalRectToScreen(logicalButtons.minimize);
        const int buttonSize = closeBtn.w;

        // Close button (red) - draw white X
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);
        SDL_RenderFillRect(renderer, &closeBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, closeBtn.x + 3, closeBtn.y + 3, closeBtn.x + buttonSize - 4, closeBtn.y + buttonSize - 4);
        SDL_RenderDrawLine(renderer, closeBtn.x + buttonSize - 4, closeBtn.y + 3, closeBtn.x + 3, closeBtn.y + buttonSize - 4);

        // Maximize (green) - draw white square outline
        SDL_SetRenderDrawColor(renderer, 60, 170, 80, 255);
        SDL_RenderFillRect(renderer, &maxBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect maxSymbol = {maxBtn.x + 3, maxBtn.y + 3, buttonSize - 7, buttonSize - 7};
        SDL_RenderDrawRect(renderer, &maxSymbol);

        // Minimize (yellow/orange) - draw white horizontal line
        SDL_SetRenderDrawColor(renderer, 200, 170, 60, 255);
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
        // Layout: top-level apps, then a Games category (Snake / Minesweeper), then Shut Down.
        // action: >=0 launch/quit, -1 category header (not clickable), -2 separator (not clickable).
        struct MenuEntry {
            const char* label;
            int action;
            int indent;  // 0 top-level, 1 nested under a category
        };
        const MenuEntry entries[] = {
            {"Terminal", 0, 0},
            {"Text Editor", 1, 0},
            {"Filesystem", 2, 0},
            {"Drawing", 4, 0},
            {"Settings", 3, 0},
            {"", -2, 0},                 // separator before Games
            {"Games", -1, 0},            // category header
            {"Snake", 5, 1},
            {"Minesweeper", 6, 1},
            {"", -2, 0},                 // separator before Shut Down
            {"Shut Down", 7, 0},
        };
        const size_t entryCount = sizeof(entries) / sizeof(entries[0]);

        const int menuWidth = 210;
        const int itemHLogical = 26;
        const int itemGapLogical = 2;
        const int categoryHLogical = 20;
        const int separatorHLogical = 10;
        const int headerHLogical = 22;
        const int topPadLogical = 6;
        const int bottomPadLogical = 8;

        int contentHLogical = topPadLogical;
        for (size_t i = 0; i < entryCount; ++i) {
            if (entries[i].action == -2) {
                contentHLogical += separatorHLogical;
            } else if (entries[i].action == -1) {
                contentHLogical += categoryHLogical + itemGapLogical;
            } else {
                contentHLogical += itemHLogical + itemGapLogical;
            }
        }
        contentHLogical += bottomPadLogical;
        const int menuHeight = headerHLogical + contentHLogical;

        const int menuX = logicalToScreenX(8);
        const int menuY = logicalToScreenY(getTaskbarRect().y - menuHeight);

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
            static_cast<int>(headerHLogical * m_contentScale)
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
            SDL_Color categoryCol = {160, 175, 200, 255};
            SDL_Color separatorCol = {70, 70, 82, 255};

            int y = menuY + static_cast<int>((headerHLogical + topPadLogical) * m_contentScale);
            const int itemH = static_cast<int>(itemHLogical * m_contentScale);
            const int categoryH = static_cast<int>(categoryHLogical * m_contentScale);
            const int separatorH = static_cast<int>(separatorHLogical * m_contentScale);
            const int itemGap = static_cast<int>(itemGapLogical * m_contentScale);
            const int basePad = static_cast<int>(10 * m_contentScale);
            const int nestPad = static_cast<int>(22 * m_contentScale);

            for (size_t i = 0; i < entryCount; ++i) {
                const auto& e = entries[i];

                if (e.action == -2) {
                    // Horizontal rule centered in the separator band
                    const int lineY = y + separatorH / 2;
                    SDL_SetRenderDrawColor(renderer, separatorCol.r, separatorCol.g, separatorCol.b, 255);
                    SDL_RenderDrawLine(
                        renderer,
                        menuX + static_cast<int>(10 * m_contentScale),
                        lineY,
                        menuX + static_cast<int>((menuWidth - 10) * m_contentScale),
                        lineY);
                    y += separatorH;
                    continue;
                }

                if (e.action == -1) {
                    // Non-clickable category header
                    SDL_Surface* s = TTF_RenderText_Blended(m_font, e.label, categoryCol);
                    if (s) {
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                        if (t) {
                            SDL_Rect dst = {
                                menuX + basePad,
                                y + (categoryH - s->h) / 2,
                                s->w, s->h
                            };
                            SDL_RenderCopy(renderer, t, nullptr, &dst);
                            SDL_DestroyTexture(t);
                        }
                        SDL_FreeSurface(s);
                    }
                    y += categoryH + itemGap;
                    continue;
                }

                // Clickable app / system row
                SDL_Rect itemRect = {
                    menuX + static_cast<int>(4 * m_contentScale),
                    y,
                    static_cast<int>((menuWidth - 8) * m_contentScale),
                    itemH
                };
                m_startMenuItems.push_back({itemRect, e.action});

                bool hovered = false;
                SDL_Point mousePt = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&mousePt, &itemRect)) {
                    hovered = true;
                    m_startMenuHoverIndex = static_cast<int>(m_startMenuItems.size() - 1);
                }

                if (hovered) {
                    SDL_SetRenderDrawColor(renderer, hoverBgCol.r, hoverBgCol.g, hoverBgCol.b, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                }

                SDL_Color useTextCol = hovered ? hoverTextCol : textCol;
                const int textPad = (e.indent > 0) ? nestPad : basePad;

                SDL_Surface* s = TTF_RenderText_Blended(m_font, e.label, useTextCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {menuX + textPad, y + (itemH - s->h) / 2, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }

                y += itemH + itemGap;
            }
        }
    }

    // === Taskbar (always visible) ===
    m_taskbarEntries.clear();

    const SDL_Rect taskbarLogicalRect = getTaskbarRect();
    const SDL_Rect taskbarRect = logicalRectToScreen(taskbarLogicalRect);
    const int taskbarY = taskbarLogicalRect.y;

    // Draw taskbar background
    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 255);
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

SDL_Rect WindowManager::getTaskbarRect() const {
    return {0, m_logicalHeight - kTaskbarHeight, m_logicalWidth, kTaskbarHeight};
}

SDL_Rect WindowManager::getUsableDesktopRect() const {
    SDL_Rect taskbar = getTaskbarRect();
    return {0, 0, m_logicalWidth, std::max(0, taskbar.y)};
}

int WindowManager::getUsableDesktopBottom() const {
    SDL_Rect usable = getUsableDesktopRect();
    return usable.y + usable.h;
}

SDL_Rect WindowManager::logicalRectToScreen(const SDL_Rect& rect) const {
    return {
        logicalToScreenX(rect.x),
        logicalToScreenY(rect.y),
        static_cast<int>(rect.w * m_contentScale),
        static_cast<int>(rect.h * m_contentScale)
    };
}

WindowManager::TitleButtonRects WindowManager::getTitleButtonRects(const Window& window) const {
    const int buttonSize = kWindowButtonSize;
    const int buttonY = window.rect.y + (Window::TITLE_BAR_HEIGHT - buttonSize) / 2;
    const int rightEdge = window.rect.x + window.rect.w - kWindowButtonRightPadding;

    TitleButtonRects rects;
    rects.close = {rightEdge - buttonSize, buttonY, buttonSize, buttonSize};
    rects.maximize = {
        rightEdge - buttonSize * 2 - kWindowButtonSpacing,
        buttonY,
        buttonSize,
        buttonSize
    };
    rects.minimize = {
        rightEdge - buttonSize * 3 - kWindowButtonSpacing * 2,
        buttonY,
        buttonSize,
        buttonSize
    };
    return rects;
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

    SDL_Point mouse = {mouseX, mouseY};
    const TitleButtonRects buttons = getTitleButtonRects(*window);

    if (SDL_PointInRect(&mouse, &buttons.close)) {
        closeWindow(window);
        return true;
    }

    if (SDL_PointInRect(&mouse, &buttons.maximize)) {
        if (window->maximized) {
            // Restore previous size/position
            window->rect = window->previousRect;
            window->maximized = false;
        } else {
            // Maximize to fill the logical desktop, leaving space for the taskbar at the bottom.
            // (the title bar sits at the top of the logical area, content fills the rest)
            window->previousRect = window->rect;

            window->rect = getUsableDesktopRect();

            window->maximized = true;
        }

        // Notify app of size change
        if (window->app) {
            const int clientH = window->rect.h - Window::TITLE_BAR_HEIGHT;
            window->app->onResize(window->rect.w, clientH > 0 ? clientH : 0);
        }
        return true;
    }

    if (SDL_PointInRect(&mouse, &buttons.minimize)) {
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
    const int desktopBottom = getUsableDesktopBottom();
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
        if (m_focusedWindow == window) m_focusedWindow = nullptr;
        if (m_draggedWindow == window) m_draggedWindow = nullptr;
        if (m_resizingWindow == window) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }

        // Clean up cached title texture for the window being closed
        auto cacheIt = m_titleCache.find(window->id);
        if (cacheIt != m_titleCache.end()) {
            if (cacheIt->second.texture) {
                SDL_DestroyTexture(cacheIt->second.texture);
            }
            m_titleCache.erase(cacheIt);
        }

        m_windows.erase(it);
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

void WindowManager::requestQuit() {
    m_quitRequested = true;
    // Menu close is handled by the Start menu click handler (or caller).
}

bool WindowManager::shouldQuit() const {
    return m_quitRequested;
}

void WindowManager::loadDesktopSettings(const std::string& hostPath) {
    m_desktopSettingsHostPath = hostPath;
    m_desktopSettings.loadFromHostPath(hostPath);
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

void WindowManager::associateEditorWithFile(Window* window, const std::string& virtualPath) {
    if (!window || virtualPath.empty() || !m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);

    auto existing = m_fileEditors.find(normalized);
    if (existing != m_fileEditors.end() && existing->second && existing->second != window) {
        bringToFront(existing->second);
        return;
    }

    if (!window->editedFilePath.empty() && window->editedFilePath != normalized) {
        auto prev = m_fileEditors.find(window->editedFilePath);
        if (prev != m_fileEditors.end() && prev->second == window) {
            m_fileEditors.erase(prev);
        }
    }

    window->editedFilePath = normalized;
    m_fileEditors[normalized] = window;
}

void WindowManager::associateDrawingWithFile(Window* window, const std::string& virtualPath) {
    if (!window || virtualPath.empty() || !m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);

    auto existing = m_fileDrawings.find(normalized);
    if (existing != m_fileDrawings.end() && existing->second && existing->second != window) {
        bringToFront(existing->second);
        return;
    }

    if (!window->drawingFilePath.empty() && window->drawingFilePath != normalized) {
        auto prev = m_fileDrawings.find(window->drawingFilePath);
        if (prev != m_fileDrawings.end() && prev->second == window) {
            m_fileDrawings.erase(prev);
        }
    }

    window->drawingFilePath = normalized;
    m_fileDrawings[normalized] = window;
}

bool WindowManager::focusEditorForFile(const std::string& virtualPath) {
    if (!m_fs) return false;
    std::string normalized = m_fs->normalize(virtualPath);
    auto it = m_fileEditors.find(normalized);
    if (it != m_fileEditors.end() && it->second) {
        bringToFront(it->second);
        return true;
    }
    return false;
}

bool WindowManager::focusDrawingForFile(const std::string& virtualPath) {
    if (!m_fs) return false;
    std::string normalized = m_fs->normalize(virtualPath);
    auto it = m_fileDrawings.find(normalized);
    if (it != m_fileDrawings.end() && it->second) {
        bringToFront(it->second);
        return true;
    }
    return false;
}

void WindowManager::clearDrawingFileBinding(Window* window) {
    if (!window || window->drawingFilePath.empty()) return;

    auto it = m_fileDrawings.find(window->drawingFilePath);
    if (it != m_fileDrawings.end() && it->second == window) {
        m_fileDrawings.erase(it);
    }
    window->drawingFilePath.clear();
}

// =============================================================================
// Per-window controller for apps (IWindowController implementation)
// =============================================================================

namespace {

std::string formatTrackedInstanceTitle(const monolith::window::Window* window) {
    if (!window || window->appBaseTitle.empty() || window->appInstanceNumber <= 0) {
        return {};
    }
    if (window->appInstanceNumber == 1) {
        return window->appBaseTitle;
    }
    return window->appBaseTitle + " " + std::to_string(window->appInstanceNumber);
}

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

    void restoreTrackedInstanceTitle() override {
        if (!targetWindow) return;
        const std::string title = formatTrackedInstanceTitle(targetWindow);
        if (!title.empty()) {
            targetWindow->title = title;
        }
    }

    void openInTextEditor(const std::string& virtualPath) override {
        if (wm) {
            wm->launchTextEditor(virtualPath);
        }
    }

    void openInDrawing(const std::string& virtualPath) override {
        if (wm) {
            wm->launchDrawing(virtualPath);
        }
    }

    bool focusEditorForFile(const std::string& virtualPath) override {
        return wm ? wm->focusEditorForFile(virtualPath) : false;
    }

    void bindEditorFile(const std::string& virtualPath) override {
        if (wm && targetWindow) {
            wm->associateEditorWithFile(targetWindow, virtualPath);
        }
    }

    void bindDrawingFile(const std::string& virtualPath) override {
        if (wm && targetWindow) {
            wm->associateDrawingWithFile(targetWindow, virtualPath);
        }
    }

    void clearDrawingFileBinding() override {
        if (wm && targetWindow) {
            wm->clearDrawingFileBinding(targetWindow);
        }
    }

    void getDesktopBackgroundColor(uint8_t& r, uint8_t& g, uint8_t& b) const override {
        if (wm) {
            auto color = wm->getDesktopBackground();
            r = color.r;
            g = color.g;
            b = color.b;
            return;
        }
        r = 25;
        g = 25;
        b = 30;
    }

    void setDesktopBackgroundColor(uint8_t r, uint8_t g, uint8_t b) override {
        if (wm) {
            wm->setDesktopBackground(r, g, b);
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
