#include "FilesystemApp.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace monolith::app {

FilesystemApp::FilesystemApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    if (m_fs) {
        // Make sure we start somewhere that exists
        if (!m_fs->isDirectory(m_currentPath)) {
            m_currentPath = "/";
            if (!m_fs->isDirectory(m_currentPath)) {
                m_currentPath = "/home";
                if (!m_fs->isDirectory(m_currentPath)) {
                    m_currentPath = "/";
                }
            }
        }
    }
    refreshEntries();
}

void FilesystemApp::setCurrentPath(const std::string& virtualPath) {
    if (!m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);
    if (m_fs->isDirectory(normalized)) {
        m_currentPath = normalized;
        refreshEntries();
        m_selectedIndex = -1;
        m_scrollOffset = 0;
    }
}

void FilesystemApp::goUp() {
    if (!m_fs) return;

    if (m_currentPath == "/" || m_currentPath.empty()) return;

    // Find parent
    size_t lastSlash = m_currentPath.find_last_of('/');
    std::string parent = (lastSlash == 0) ? "/" : m_currentPath.substr(0, lastSlash);
    if (parent.empty()) parent = "/";

    setCurrentPath(parent);
}

void FilesystemApp::refreshEntries() {
    m_entries.clear();
    if (!m_fs) return;

    auto raw = m_fs->listEntries(m_currentPath);
    m_entries = std::move(raw);

    // Always allow ".." unless we're at root
    // (we insert it visually at the front during render or as a real entry)
}

std::string FilesystemApp::fullPathFor(const std::string& name) const {
    if (m_currentPath == "/") {
        return "/" + name;
    }
    return m_currentPath + "/" + name;
}

void FilesystemApp::activateEntry(size_t index) {
    if (index >= m_entries.size()) return;

    const auto& entry = m_entries[index];

    if (entry.isDirectory) {
        std::string newPath = fullPathFor(entry.name);
        setCurrentPath(newPath);
    } else {
        // File: ask the shell to open it in the text editor
        if (auto* ctrl = getController()) {
            ctrl->openInTextEditor(fullPathFor(entry.name));
        }
    }
}

void FilesystemApp::createNewFolder() {
    if (!m_fs) return;

    std::string base = "New Folder";
    std::string candidate = base;
    int counter = 2;

    // Find a free name
    while (m_fs->exists(fullPathFor(candidate))) {
        std::ostringstream oss;
        oss << base << " " << counter++;
        candidate = oss.str();
    }

    if (m_fs->createDirectory(fullPathFor(candidate))) {
        refreshEntries();

        // Select the newly created folder
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].name == candidate && m_entries[i].isDirectory) {
                setSelection(static_cast<int>(i));
                ensureSelectionVisible();
                break;
            }
        }
    }
}

void FilesystemApp::createNewFile() {
    if (!m_fs) return;

    std::string base = "New File.txt";
    std::string candidate = base;
    int counter = 2;

    while (m_fs->exists(fullPathFor(candidate))) {
        std::ostringstream oss;
        oss << "New File " << counter++ << ".txt";
        candidate = oss.str();
    }

    if (m_fs->writeFile(fullPathFor(candidate), "")) {
        refreshEntries();

        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].name == candidate && !m_entries[i].isDirectory) {
                setSelection(static_cast<int>(i));
                ensureSelectionVisible();
                // Immediately allow renaming the new file (nice UX)
                startRenameSelected();
                break;
            }
        }
    }
}

void FilesystemApp::deleteSelected() {
    if (!m_fs || m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_entries.size())) {
        return;
    }

    const auto& entry = m_entries[m_selectedIndex];
    std::string target = fullPathFor(entry.name);

    // Simple delete (no confirmation dialog for v1)
    if (m_fs->remove(target)) {
        int oldSel = m_selectedIndex;
        refreshEntries();

        // Try to keep a reasonable selection
        if (!m_entries.empty()) {
            int newSel = std::min(oldSel, static_cast<int>(m_entries.size()) - 1);
            setSelection(newSel);
        } else {
            m_selectedIndex = -1;
        }
        ensureSelectionVisible();
    }
}

void FilesystemApp::startRenameSelected() {
    if (!m_fs || m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_entries.size())) {
        return;
    }

    m_renaming = true;
    m_renameIndex = m_selectedIndex;
    m_renameBuffer = m_entries[m_selectedIndex].name;
}

void FilesystemApp::finishRename(bool commit) {
    if (!m_renaming || m_renameIndex < 0 || m_renameIndex >= static_cast<int>(m_entries.size())) {
        m_renaming = false;
        m_renameIndex = -1;
        m_renameBuffer.clear();
        return;
    }

    const auto& entry = m_entries[m_renameIndex];
    std::string oldPath = fullPathFor(entry.name);

    if (commit && !m_renameBuffer.empty() && m_renameBuffer != entry.name) {
        std::string newPath = fullPathFor(m_renameBuffer);

        // Prevent naming conflicts
        if (!m_fs->exists(newPath)) {
            if (m_fs->rename(oldPath, newPath)) {
                refreshEntries();

                // Try to reselect the renamed item
                for (size_t i = 0; i < m_entries.size(); ++i) {
                    if (m_entries[i].name == m_renameBuffer) {
                        setSelection(static_cast<int>(i));
                        break;
                    }
                }
            }
        }
    }

    m_renaming = false;
    m_renameIndex = -1;
    m_renameBuffer.clear();
}

void FilesystemApp::startNewFileNaming() {
    if (!m_fs) return;
    m_namingNewFile = true;
    m_newFileNameBuffer = "New File.txt";
}

void FilesystemApp::finishNewFileNaming(bool commit) {
    if (!m_namingNewFile) return;

    if (commit && !m_newFileNameBuffer.empty()) {
        std::string candidate = m_newFileNameBuffer;
        int counter = 2;

        while (m_fs->exists(fullPathFor(candidate))) {
            std::ostringstream oss;
            size_t dot = m_newFileNameBuffer.find_last_of('.');
            if (dot != std::string::npos) {
                oss << m_newFileNameBuffer.substr(0, dot) << " " << counter << m_newFileNameBuffer.substr(dot);
            } else {
                oss << m_newFileNameBuffer << " " << counter;
            }
            candidate = oss.str();
            counter++;
        }

        if (m_fs->writeFile(fullPathFor(candidate), "")) {
            refreshEntries();
            for (size_t i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].name == candidate && !m_entries[i].isDirectory) {
                    setSelection(static_cast<int>(i));
                    ensureSelectionVisible();
                    break;
                }
            }
        }
    }

    m_namingNewFile = false;
    m_newFileNameBuffer.clear();
}

void FilesystemApp::setSelection(int index) {
    if (m_entries.empty()) {
        m_selectedIndex = -1;
        return;
    }
    if (index < 0) index = 0;
    if (index >= static_cast<int>(m_entries.size())) index = static_cast<int>(m_entries.size()) - 1;
    m_selectedIndex = index;
}

void FilesystemApp::ensureSelectionVisible() {
    if (m_selectedIndex < 0) return;

    int visible = getVisibleRowCount({0, 0, m_clientWidth, m_clientHeight});
    if (visible <= 0) visible = 8;

    if (m_selectedIndex < m_scrollOffset) {
        m_scrollOffset = m_selectedIndex;
    } else if (m_selectedIndex >= m_scrollOffset + visible) {
        m_scrollOffset = m_selectedIndex - visible + 1;
    }
    if (m_scrollOffset < 0) m_scrollOffset = 0;
}

int FilesystemApp::getVisibleRowCount(const SDL_Rect& contentRect) const {
    int rh = getRowHeight();
    if (rh <= 0) return 10;

    // Leave some space for path bar + toolbar area at top and status bar at bottom
    const int reservedTop = 52;   // path bar + small toolbar strip
    const int reservedBottom = 22 + 8; // status bar + padding
    int available = contentRect.h - reservedTop - reservedBottom;
    if (available < 20) available = 20;

    return std::max(3, available / rh);
}

void FilesystemApp::handleMouseButton(const SDL_MouseButtonEvent& e, const SDL_Rect& contentRect) {
    const int mx = e.x;
    const int my = e.y;

    // === Context menu handling (takes priority) ===
    if (m_showContextMenu) {
        if (e.button == SDL_BUTTON_LEFT) {
            // Check if clicking on a menu item
            // Menu is drawn at m_contextMenuPos (relative)
            int menuX = m_contextMenuPos.x;
            int menuY = m_contextMenuPos.y;
            int itemHeight = getRowHeight() + 2;
            int padding = 6;

            int clickedItem = -1;
            int itemY = menuY + padding;
            for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
                SDL_Rect itemRect = {menuX, itemY, m_contextMenuWidth, itemHeight};
                if (mx >= itemRect.x && mx < itemRect.x + m_contextMenuWidth &&
                    my >= itemRect.y && my < itemRect.y + itemHeight) {
                    clickedItem = static_cast<int>(i);
                    break;
                }
                itemY += itemHeight;
            }

            if (clickedItem != -1) {
                executeContextMenuAction(clickedItem);
            } else {
                closeContextMenu();
            }
        } else if (e.button == SDL_BUTTON_RIGHT) {
            closeContextMenu();
        }
        return;
    }

    // === Right click → show context menu ===
    if (e.button == SDL_BUTTON_RIGHT) {
        std::cout << "[FilesystemApp] RIGHT CLICK detected at relative (" << mx << "," << my << ")" << std::endl;

        const int listTop = 52;

        if (my >= listTop) {
            int rowHeight = getRowHeight();
            if (rowHeight <= 0) rowHeight = 20;

            int relY = my - listTop;
            int clickedRow = m_scrollOffset + (relY / rowHeight);

            if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
                setSelection(clickedRow);
                std::cout << "[FilesystemApp] Right-click on item " << clickedRow << std::endl;
                showContextMenu(mx, my, clickedRow);
            } else {
                std::cout << "[FilesystemApp] Right-click on empty space (background menu)" << std::endl;
                m_selectedIndex = -1;
                showContextMenu(mx, my, -1);
            }
        } else {
            std::cout << "[FilesystemApp] Right-click above list (background menu)" << std::endl;
            m_selectedIndex = -1;
            showContextMenu(mx, my, -1);
        }
        return;
    }

    // === Left click handling ===
    if (e.button != SDL_BUTTON_LEFT) return;

    // Check toolbar buttons first
    SDL_Point pt { mx, my };
    if (SDL_PointInRect(&pt, &m_btnUp)) {
        goUp();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnNewFolder)) {
        createNewFolder();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnNewFile)) {
        createNewFile();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnDelete)) {
        deleteSelected();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnRename)) {
        startRenameSelected();
        return;
    }

    // If we click anywhere while renaming, finish (cancel) the rename
    if (m_renaming) {
        finishRename(false);
    }

    // Path bar + toolbar area is above the list
    const int listTop = 52;
    if (my < listTop) return;

    // Compute which row was clicked
    int rowHeight = getRowHeight();
    if (rowHeight <= 0) rowHeight = 20;

    int relY = my - listTop;
    if (relY < 0) return;

    int clickedRow = m_scrollOffset + (relY / rowHeight);
    if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
        setSelection(clickedRow);

        if (e.clicks >= 2) {
            activateEntry(static_cast<size_t>(clickedRow));
        }
    } else {
        m_selectedIndex = -1;
    }
}

void FilesystemApp::handleMouseWheel(const SDL_MouseWheelEvent& e) {
    int visible = getVisibleRowCount({0, 0, m_clientWidth, m_clientHeight});
    int maxScroll = std::max(0, static_cast<int>(m_entries.size()) - visible);

    m_scrollOffset -= e.y * 3; // a few rows per wheel tick
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
}

void FilesystemApp::handleKeyDown(const SDL_Keysym& keysym) {
    if (m_renaming) {
        // Handle rename mode specially
        if (keysym.sym == SDLK_RETURN || keysym.sym == SDLK_KP_ENTER) {
            finishRename(true);
            return;
        }
        if (keysym.sym == SDLK_ESCAPE) {
            finishRename(false);
            return;
        }
        if (keysym.sym == SDLK_BACKSPACE) {
            if (!m_renameBuffer.empty()) {
                m_renameBuffer.pop_back();
            }
            return;
        }
        return; // Ignore other keys while renaming
    }

    switch (keysym.sym) {
        case SDLK_UP:
            if (m_selectedIndex > 0) {
                setSelection(m_selectedIndex - 1);
                ensureSelectionVisible();
            }
            break;

        case SDLK_DOWN:
            if (m_selectedIndex + 1 < static_cast<int>(m_entries.size())) {
                setSelection(m_selectedIndex + 1);
                ensureSelectionVisible();
            } else if (m_selectedIndex < 0 && !m_entries.empty()) {
                setSelection(0);
                ensureSelectionVisible();
            }
            break;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (m_selectedIndex >= 0) {
                activateEntry(static_cast<size_t>(m_selectedIndex));
            }
            break;

        case SDLK_BACKSPACE:
            goUp();
            break;

        case SDLK_DELETE:
            deleteSelected();
            break;

        case SDLK_F2:
            startRenameSelected();
            break;

        case SDLK_F5:
            refreshEntries();
            break;

        case SDLK_ESCAPE:
            if (m_showContextMenu) {
                closeContextMenu();
            } else if (m_renaming) {
                finishRename(false);
            } else if (m_namingNewFile) {
                finishNewFileNaming(false);
            }
            break;

        default:
            break;
    }
}

void FilesystemApp::handleEvent(const SDL_Event& event) {
    if (m_renaming && event.type == SDL_TEXTINPUT) {
        if (event.text.text) {
            m_renameBuffer += event.text.text;
        }
        return;
    }

    if (m_namingNewFile && event.type == SDL_TEXTINPUT) {
        if (event.text.text) {
            m_newFileNameBuffer += event.text.text;
        }
        return;
    }

    if (event.type == SDL_MOUSEMOTION && m_showContextMenu) {
        // Update hover for context menu
        int mx = event.motion.x;
        int my = event.motion.y;

        int menuX = m_contextMenuPos.x;
        int menuY = m_contextMenuPos.y;
        int itemHeight = getRowHeight() + 2;
        int padding = 6;

        m_contextMenuHoverIndex = -1;
        int itemY = menuY + padding;
        for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
            if (mx >= menuX && mx < menuX + 200 &&
                my >= itemY && my < itemY + itemHeight) {
                m_contextMenuHoverIndex = static_cast<int>(i);
                break;
            }
            itemY += itemHeight;
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        SDL_Rect fake{0, 0, m_clientWidth, m_clientHeight};
        handleMouseButton(event.button, fake);
    } else if (event.type == SDL_MOUSEWHEEL) {
        handleMouseWheel(event.wheel);
    } else if (event.type == SDL_KEYDOWN) {
        handleKeyDown(event.key.keysym);
    }
}

int FilesystemApp::getRowHeight() const {
    if (!m_font) return 20;
    return TTF_FontHeight(m_font) + 4; // small padding between rows
}

void FilesystemApp::drawPathBar(SDL_Renderer* r, const SDL_Rect& contentRect, int& outTopY) {
    const int barHeight = 28;
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        barHeight
    };

    // Slightly different background for the path area
    SDL_SetRenderDrawColor(r, 28, 30, 36, 255);
    SDL_RenderFillRect(r, &bar);

    // Current path text
    if (m_font) {
        SDL_Color pathColor = {180, 190, 200, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, m_currentPath.c_str(), pathColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect dst = {
                    contentRect.x + 10,
                    contentRect.y + (barHeight - surf->h) / 2,
                    std::min(surf->w, contentRect.w - 120),
                    surf->h
                };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    outTopY = contentRect.y + barHeight;
}

void FilesystemApp::drawToolbar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    // Store button rects in relative coordinates (for hit testing with relative mouse events)
    const int relY = 30;
    const int h = 20;
    const int padding = 8;
    const int gap = 6;

    int relX = padding;

    auto drawButton = [&](SDL_Rect& outRect, const char* label, int w) {
        // Store relative rect for input
        outRect = {relX, relY, w, h};

        // Draw using absolute screen coordinates
        SDL_Rect drawRect = {
            contentRect.x + relX,
            contentRect.y + relY,
            w,
            h
        };

        // Button background
        SDL_SetRenderDrawColor(r, 45, 48, 55, 255);
        SDL_RenderFillRect(r, &drawRect);

        // Subtle border
        SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
        SDL_RenderDrawRect(r, &drawRect);

        if (m_font) {
            SDL_Color col = {210, 215, 220, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, label, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_Rect dst = {
                        drawRect.x + (drawRect.w - surf->w) / 2,
                        drawRect.y + (drawRect.h - surf->h) / 2,
                        surf->w, surf->h
                    };
                    SDL_RenderCopy(r, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        relX += w + gap;
    };

    drawButton(m_btnUp, "Up", 48);
    drawButton(m_btnNewFolder, "New Folder", 92);
    drawButton(m_btnNewFile, "New File", 80);
    drawButton(m_btnRename, "Rename", 68);
    drawButton(m_btnDelete, "Delete", 68);
}

void FilesystemApp::drawList(SDL_Renderer* r, const SDL_Rect& contentRect, int listTopY) {
    if (!m_font) {
        SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
        SDL_RenderFillRect(r, &contentRect);
        return;
    }

    const int rowH = getRowHeight();
    (void)rowH; // used via getVisibleRowCount

    const int statusBarHeight = 22;
    const int listHeight = contentRect.h - (listTopY - contentRect.y) - 6 - statusBarHeight;

    SDL_Rect listArea = {
        contentRect.x,
        listTopY,
        contentRect.w,
        listHeight
    };

    // List background
    SDL_SetRenderDrawColor(r, 22, 22, 26, 255);
    SDL_RenderFillRect(r, &listArea);

    if (m_entries.empty()) {
        SDL_Color dim = {140, 145, 150, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, "(empty directory)", dim);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect dst = {listArea.x + 16, listArea.y + 10, surf->w, surf->h};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        return;
    }

    SDL_Color textNormal   = {205, 210, 215, 255};
    SDL_Color textDir      = {170, 200, 240, 255};  // slightly blue for directories
    SDL_Color selBg        = {55, 70, 95, 255};
    SDL_Color selText      = {230, 235, 245, 255};

    int y = listTopY + 2;
    int visible = getVisibleRowCount(contentRect);

    for (int i = 0; i < visible; ++i) {
        int entryIdx = m_scrollOffset + i;
        if (entryIdx >= static_cast<int>(m_entries.size())) break;

        const auto& entry = m_entries[entryIdx];
        bool isSelected = (entryIdx == m_selectedIndex);
        bool isRenamingThis = m_renaming && (entryIdx == m_renameIndex);

        SDL_Rect rowRect = {
            listArea.x,
            y,
            listArea.w,
            rowH - 1
        };

        if (isSelected && !isRenamingThis) {
            SDL_SetRenderDrawColor(r, selBg.r, selBg.g, selBg.b, 255);
            SDL_RenderFillRect(r, &rowRect);
        }

        if (isRenamingThis) {
            // Highlight rename row more strongly
            SDL_SetRenderDrawColor(r, 70, 90, 120, 255);
            SDL_RenderFillRect(r, &rowRect);
        }

        // Small indicator for directory vs file
        const char* indicator = entry.isDirectory ? "▶" : "•";
        SDL_Color indColor = entry.isDirectory ? textDir : textNormal;

        // Indicator
        {
            SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, indicator, indColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
                if (t) {
                    SDL_Rect d = {rowRect.x + 8, rowRect.y + 2, s->w, s->h};
                    SDL_RenderCopy(r, t, nullptr, &d);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Name (or rename buffer if renaming this row)
        {
            std::string displayText = isRenamingThis ? m_renameBuffer : entry.name;
            SDL_Color nameCol = isRenamingThis ? selText : (isSelected ? selText : (entry.isDirectory ? textDir : textNormal));

            SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, displayText.c_str(), nameCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
                if (t) {
                    int maxW = rowRect.w - 40;
                    SDL_Rect d = {rowRect.x + 28, rowRect.y + 2, std::min(s->w, maxW), s->h};
                    SDL_RenderCopy(r, t, nullptr, &d);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }

            // Draw a simple cursor when renaming
            if (isRenamingThis) {
                int textW = 0, textH = 0;
                if (!displayText.empty()) {
                    TTF_SizeUTF8(m_font, displayText.c_str(), &textW, &textH);
                }
                int cursorX = rowRect.x + 28 + textW + 1;
                int cursorY = rowRect.y + 2;
                SDL_SetRenderDrawColor(r, 255, 255, 255, 220);
                SDL_RenderDrawLine(r, cursorX, cursorY, cursorX, cursorY + rowH - 6);
            }
        }

        y += rowH;
    }
}

void FilesystemApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    m_clientWidth = contentRect.w;
    m_clientHeight = contentRect.h;

    // Main background
    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    if (!m_font) {
        return;
    }

    int listTop = 0;
    drawPathBar(renderer, contentRect, listTop);
    drawToolbar(renderer, contentRect);
    drawList(renderer, contentRect, listTop + 22);  // toolbar sits just under path bar
    drawStatusBar(renderer, contentRect);

    if (m_showContextMenu) {
        drawContextMenu(renderer, contentRect);
    }
}

void FilesystemApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    ensureSelectionVisible();
}

void FilesystemApp::showContextMenu(int x, int y, int targetIndex) {
    closeContextMenu(); // close any existing menu first

    m_contextMenuPos = {x, y};
    m_contextMenuTarget = targetIndex;
    m_contextMenuHoverIndex = -1;
    m_contextMenuItems.clear();

    if (targetIndex == -1) {
        // Background menu
        m_contextMenuItems.push_back("New Folder");
        m_contextMenuItems.push_back("New File");
        m_contextMenuItems.push_back("Refresh");
    } else if (targetIndex >= 0 && targetIndex < static_cast<int>(m_entries.size())) {
        const auto& entry = m_entries[targetIndex];
        m_contextMenuItems.push_back("Open");
        m_contextMenuItems.push_back("Rename");
        if (entry.isDirectory) {
            // Could add more folder-specific options later
        }
        m_contextMenuItems.push_back("Delete");
    }

    // Calculate menu dimensions (same logic as drawContextMenu)
    if (!m_contextMenuItems.empty() && m_font) {
        const int itemHeight = getRowHeight() + 2;
        const int padding = 6;
        const int minWidth = 140;

        int maxTextWidth = minWidth;
        for (const auto& item : m_contextMenuItems) {
            int w = 0, h = 0;
            TTF_SizeUTF8(m_font, item.c_str(), &w, &h);
            if (w > maxTextWidth) maxTextWidth = w;
        }

        m_contextMenuWidth = maxTextWidth + padding * 2 + 20;
        m_contextMenuHeight = static_cast<int>(m_contextMenuItems.size()) * itemHeight + padding * 2;
    } else {
        m_contextMenuWidth = 0;
        m_contextMenuHeight = 0;
    }

    m_showContextMenu = true;

    std::cout << "[FilesystemApp] showContextMenu called for target=" << targetIndex 
              << " at (" << x << "," << y << ") with " << m_contextMenuItems.size() << " items" << std::endl;
}

void FilesystemApp::closeContextMenu() {
    m_showContextMenu = false;
    m_contextMenuItems.clear();
    m_contextMenuHoverIndex = -1;
    m_contextMenuTarget = -1;
}

void FilesystemApp::executeContextMenuAction(int menuIndex) {
    if (!m_showContextMenu || menuIndex < 0 || menuIndex >= static_cast<int>(m_contextMenuItems.size())) {
        closeContextMenu();
        return;
    }

    std::string action = m_contextMenuItems[menuIndex];
    int target = m_contextMenuTarget;   // Save before closing!
    closeContextMenu();

    if (action == "New Folder") {
        createNewFolder();
    } else if (action == "New File") {
        createNewFile();
    } else if (action == "Refresh") {
        refreshEntries();
    } else if (action == "Open") {
        if (target >= 0) {
            activateEntry(target);
        }
    } else if (action == "Rename") {
        if (target >= 0) {
            m_selectedIndex = target;
            startRenameSelected();
        }
    } else if (action == "Delete") {
        if (target >= 0) {
            m_selectedIndex = target;
            if (m_confirmingDelete) {
                deleteSelected();
                m_confirmingDelete = false;
            } else {
                m_confirmingDelete = true;
                // Re-show menu with confirmation
                int mx = m_contextMenuPos.x;
                int my = m_contextMenuPos.y;
                showContextMenu(mx, my, target);
                m_contextMenuItems.clear();
                m_contextMenuItems.push_back("Confirm Delete");
                m_contextMenuItems.push_back("Cancel");
            }
        }
    } else if (action == "Confirm Delete") {
        deleteSelected();
        m_confirmingDelete = false;
    } else if (action == "Cancel") {
        m_confirmingDelete = false;
    }
}

void FilesystemApp::drawStatusBar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    const int statusH = 22;
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y + contentRect.h - statusH,
        contentRect.w,
        statusH
    };

    SDL_SetRenderDrawColor(r, 30, 32, 38, 255);
    SDL_RenderFillRect(r, &bar);

    if (!m_font) return;

    std::string status;

    if (m_entries.empty()) {
        status = "0 items";
    } else {
        status = std::to_string(m_entries.size()) + " items";
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size())) {
        const auto& sel = m_entries[m_selectedIndex];
        status += "   |   Selected: " + sel.name;
        if (sel.isDirectory) status += " (dir)";
    }

    SDL_Color textCol = {160, 165, 175, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, status.c_str(), textCol);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        if (tex) {
            SDL_Rect dst = {
                contentRect.x + 10,
                bar.y + (statusH - surf->h) / 2,
                surf->w,
                surf->h
            };
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void FilesystemApp::drawContextMenu(SDL_Renderer* r, const SDL_Rect& contentRect) {
    if (!m_showContextMenu || m_contextMenuItems.empty() || !m_font) return;

    std::cout << "[FilesystemApp] DRAWING context menu with " << m_contextMenuItems.size() 
              << " items at relative (" << m_contextMenuPos.x << "," << m_contextMenuPos.y << ")" << std::endl;

    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;
    const int minWidth = 140;

    // Calculate menu size
    int maxTextWidth = minWidth;
    for (const auto& item : m_contextMenuItems) {
        int w = 0, h = 0;
        TTF_SizeUTF8(m_font, item.c_str(), &w, &h);
        if (w > maxTextWidth) maxTextWidth = w;
    }

    int menuWidth = maxTextWidth + padding * 2 + 20;
    int menuHeight = static_cast<int>(m_contextMenuItems.size()) * itemHeight + padding * 2;

    // Convert relative click position to absolute screen coordinates
    int menuX = contentRect.x + m_contextMenuPos.x;
    int menuY = contentRect.y + m_contextMenuPos.y;

    // Simple clamping
    if (menuX + menuWidth > contentRect.x + m_clientWidth)  menuX = contentRect.x + m_clientWidth - menuWidth - 4;
    if (menuY + menuHeight > contentRect.y + m_clientHeight) menuY = contentRect.y + m_clientHeight - menuHeight - 4;
    if (menuX < contentRect.x + 4) menuX = contentRect.x + 4;
    if (menuY < contentRect.y + 4) menuY = contentRect.y + 4;

    SDL_Rect menuRect = {menuX, menuY, menuWidth, menuHeight};

    // Background
    SDL_SetRenderDrawColor(r, 38, 40, 48, 255);
    SDL_RenderFillRect(r, &menuRect);

    // Border
    SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
    SDL_RenderDrawRect(r, &menuRect);

    // Menu items
    int itemY = menuY + padding;
    for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
        bool hovered = (static_cast<int>(i) == m_contextMenuHoverIndex);

        SDL_Rect itemRect = {menuX + 1, itemY, menuWidth - 2, itemHeight - 1};

        if (hovered) {
            SDL_SetRenderDrawColor(r, 60, 80, 110, 255);
            SDL_RenderFillRect(r, &itemRect);
        }

        SDL_Color textCol = hovered ? SDL_Color{230, 235, 245, 255} : SDL_Color{200, 205, 215, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, m_contextMenuItems[i].c_str(), textCol);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect dst = {
                    menuX + padding + 4,
                    itemY + (itemHeight - surf->h) / 2,
                    surf->w, surf->h
                };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        itemY += itemHeight;
    }
}

} // namespace monolith::app
