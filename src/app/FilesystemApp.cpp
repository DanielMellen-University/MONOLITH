#include "FilesystemApp.hpp"

#include <algorithm>
#include <sstream>

namespace monolith::app {

namespace {
constexpr int kPathBarHeight = 28;
constexpr int kToolbarY = 30;
constexpr int kToolbarButtonHeight = 20;
constexpr int kToolbarPadding = 8;
constexpr int kToolbarGap = 6;
constexpr int kListTop = 52;
constexpr int kStatusBarHeight = 22;
constexpr int kStatusBarPadding = 8;
constexpr int kMinimumListHeight = 20;
}

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
    if (!m_fs) {
        setStatus("Filesystem not available");
        return;
    }

    std::string normalized = m_fs->normalize(virtualPath);
    if (m_fs->isDirectory(normalized)) {
        m_currentPath = normalized;
        refreshEntries();
        m_selectedIndex = -1;
        m_scrollOffset = 0;
        setStatus("Opened: " + m_currentPath);
    } else {
        setStatus("Open failed: not a directory");
    }
}

void FilesystemApp::goUp() {
    if (!m_fs) {
        setStatus("Filesystem not available");
        return;
    }

    if (m_currentPath == "/" || m_currentPath.empty()) return;

    // Compute parent via .. + normalize (reuses FS canonicalization for robustness;
    // matches the join+normalize style used in Terminal for consistent path rules).
    std::string candidate = m_currentPath + "/..";
    std::string parent = m_fs->normalize(candidate);
    if (parent.empty()) parent = "/";

    setCurrentPath(parent);
}

void FilesystemApp::refreshEntries() {
    std::string selectedName;
    bool selectedIsDirectory = false;
    bool hadSelection = m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size());
    if (hadSelection) {
        selectedName = m_entries[m_selectedIndex].name;
        selectedIsDirectory = m_entries[m_selectedIndex].isDirectory;
    }

    m_entries.clear();
    if (!m_fs) {
        m_selectedIndex = -1;
        return;
    }

    auto raw = m_fs->listEntries(m_currentPath);
    m_entries = std::move(raw);

    if (hadSelection && selectEntryNamed(selectedName, selectedIsDirectory)) {
        return;
    }
    clampSelection();
}

std::string FilesystemApp::fullPathFor(const std::string& name) const {
    // Use normalize after construction for consistent rules with Terminal (no raw manual glue).
    if (m_currentPath == "/" || m_currentPath.empty()) {
        return m_fs ? m_fs->normalize("/" + name) : ("/" + name);
    }
    return m_fs ? m_fs->normalize(m_currentPath + "/" + name) : (m_currentPath + "/" + name);
}

void FilesystemApp::openFileEntry(const std::string& name) {
    if (!getController()) return;

    const std::string path = fullPathFor(name);
    auto* ctrl = getController();
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".modr") == 0) {
        ctrl->openInDrawing(path);
    } else {
        ctrl->openInTextEditor(path);
    }
}

void FilesystemApp::activateEntry(size_t index) {
    if (index >= m_entries.size()) return;

    const auto& entry = m_entries[index];

    if (entry.isDirectory) {
        std::string newPath = fullPathFor(entry.name);
        setCurrentPath(newPath);
    } else {
        openFileEntry(entry.name);
    }
}

void FilesystemApp::createNewFolder() {
    if (!m_fs) {
        setStatus("Create folder failed: filesystem not available");
        return;
    }

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
        selectEntryNamed(candidate, true);
        setStatus("Created folder: " + candidate);
    } else {
        setStatus("Create folder failed: " + candidate);
    }
}

void FilesystemApp::createNewFile() {
    if (!m_fs) {
        setStatus("Create file failed: filesystem not available");
        return;
    }

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
        if (selectEntryNamed(candidate, false)) {
            startRenameSelected();
            setStatus("Created file: " + candidate);
        }
    } else {
        setStatus("Create file failed: " + candidate);
    }
}

void FilesystemApp::deleteSelected() {
    if (!m_fs || m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_entries.size())) {
        setStatus(m_fs ? "Delete failed: no item selected" : "Delete failed: filesystem not available");
        return;
    }

    const std::string name = m_entries[m_selectedIndex].name;
    std::string target = fullPathFor(name);

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
        setStatus("Deleted: " + name);
    } else {
        setStatus("Delete failed: " + name);
    }
}

void FilesystemApp::startRenameSelected() {
    if (!m_fs || m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_entries.size())) {
        setStatus(m_fs ? "Rename failed: no item selected" : "Rename failed: filesystem not available");
        return;
    }

    m_renaming = true;
    m_renameIndex = m_selectedIndex;
    m_renameBuffer = m_entries[m_selectedIndex].name;
    setStatus("Renaming: " + m_renameBuffer);
}

void FilesystemApp::finishRename(bool commit) {
    if (!m_renaming || m_renameIndex < 0 || m_renameIndex >= static_cast<int>(m_entries.size())) {
        m_renaming = false;
        m_renameIndex = -1;
        m_renameBuffer.clear();
        return;
    }

    const auto& entry = m_entries[m_renameIndex];
    std::string oldName = entry.name;
    bool wasDirectory = entry.isDirectory;
    std::string oldPath = fullPathFor(entry.name);

    if (commit && m_renameBuffer.empty()) {
        setStatus("Rename failed: name cannot be empty");
    } else if (commit && m_renameBuffer != entry.name) {
        std::string newPath = fullPathFor(m_renameBuffer);

        if (m_fs->exists(newPath)) {
            setStatus("Rename failed: name already exists");
        } else if (m_fs->rename(oldPath, newPath)) {
            refreshEntries();
            selectEntryNamed(m_renameBuffer, wasDirectory);
            setStatus("Renamed " + oldName + " to " + m_renameBuffer);
        } else {
            setStatus("Rename failed: " + oldName);
        }
    } else if (!commit) {
        setStatus("Rename canceled");
    } else {
        setStatus("Rename unchanged");
    }

    m_renaming = false;
    m_renameIndex = -1;
    m_renameBuffer.clear();
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

bool FilesystemApp::selectEntryNamed(const std::string& name, bool isDirectory) {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].name == name && m_entries[i].isDirectory == isDirectory) {
            setSelection(static_cast<int>(i));
            ensureSelectionVisible();
            return true;
        }
    }
    return false;
}

void FilesystemApp::clampSelection() {
    if (m_entries.empty()) {
        m_selectedIndex = -1;
        m_scrollOffset = 0;
        return;
    }
    if (m_selectedIndex >= static_cast<int>(m_entries.size())) {
        m_selectedIndex = static_cast<int>(m_entries.size()) - 1;
    }
    if (m_selectedIndex < -1) {
        m_selectedIndex = -1;
    }
    ensureSelectionVisible();
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

    const int reservedTop = kListTop;
    const int reservedBottom = kStatusBarHeight + kStatusBarPadding;
    int available = contentRect.h - reservedTop - reservedBottom;
    if (available < kMinimumListHeight) available = kMinimumListHeight;

    return std::max(3, available / rh);
}

void FilesystemApp::handleMouseButton(const SDL_MouseButtonEvent& e) {
    const int mx = e.x;
    const int my = e.y;

    // === Context menu handling (takes priority) ===
    if (m_showContextMenu) {
        if (e.button == SDL_BUTTON_LEFT) {
            int clickedItem = contextMenuItemAt(mx, my);
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
        if (my >= kListTop) {
            int rowHeight = getRowHeight();
            if (rowHeight <= 0) rowHeight = 20;

            int relY = my - kListTop;
            int clickedRow = m_scrollOffset + (relY / rowHeight);

            if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
                setSelection(clickedRow);
                showContextMenu(mx, my, clickedRow);
            } else {
                m_selectedIndex = -1;
                showContextMenu(mx, my, -1);
            }
        } else {
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
    if (my < kListTop) return;

    // Compute which row was clicked
    int rowHeight = getRowHeight();
    if (rowHeight <= 0) rowHeight = 20;

    int relY = my - kListTop;
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
            setStatus("Refreshed");
            break;

        case SDLK_ESCAPE:
            if (m_showContextMenu) {
                closeContextMenu();
            } else if (m_renaming) {
                finishRename(false);
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

    if (event.type == SDL_MOUSEMOTION && m_showContextMenu) {
        m_contextMenuHoverIndex = contextMenuItemAt(event.motion.x, event.motion.y);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        handleMouseButton(event.button);
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
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        kPathBarHeight
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
                    contentRect.y + (kPathBarHeight - surf->h) / 2,
                    std::min(surf->w, contentRect.w - 120),
                    surf->h
                };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    outTopY = contentRect.y + kPathBarHeight;
}

void FilesystemApp::drawToolbar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    // Store button rects in relative coordinates (for hit testing with relative mouse events)
    int relX = kToolbarPadding;

    auto drawButton = [&](SDL_Rect& outRect, const char* label, int w) {
        // Store relative rect for input
        outRect = {relX, kToolbarY, w, kToolbarButtonHeight};

        // Draw using absolute screen coordinates
        SDL_Rect drawRect = {
            contentRect.x + relX,
            contentRect.y + kToolbarY,
            w,
            kToolbarButtonHeight
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

        relX += w + kToolbarGap;
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

    const int listHeight = contentRect.h - (listTopY - contentRect.y) - 6 - kStatusBarHeight;

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
    drawList(renderer, contentRect, contentRect.y + kListTop);
    drawStatusBar(renderer, contentRect);

    if (m_showContextMenu) {
        drawContextMenu(renderer, contentRect);
    }
}

void FilesystemApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    ensureSelectionVisible();
    if (m_showContextMenu) {
        updateContextMenuLayout();
    }
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

    updateContextMenuLayout();
    m_showContextMenu = !m_contextMenuItems.empty();
}

void FilesystemApp::closeContextMenu() {
    m_showContextMenu = false;
    m_confirmingDelete = false;
    m_contextMenuItems.clear();
    m_contextMenuHoverIndex = -1;
    m_contextMenuTarget = -1;
    m_contextMenuRect = {0, 0, 0, 0};
}

void FilesystemApp::executeContextMenuAction(int menuIndex) {
    if (!m_showContextMenu || menuIndex < 0 || menuIndex >= static_cast<int>(m_contextMenuItems.size())) {
        closeContextMenu();
        return;
    }

    std::string action = m_contextMenuItems[menuIndex];
    int target = m_contextMenuTarget;   // Save before closing!
    SDL_Point menuPos = m_contextMenuPos;
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
            m_confirmingDelete = true;
            m_contextMenuPos = menuPos;
            m_contextMenuTarget = target;
            m_contextMenuItems.clear();
            m_contextMenuItems.push_back("Confirm Delete");
            m_contextMenuItems.push_back("Cancel");
            updateContextMenuLayout();
            m_showContextMenu = true;
        }
    } else if (action == "Confirm Delete") {
        if (target >= 0) {
            m_selectedIndex = target;
        }
        deleteSelected();
        m_confirmingDelete = false;
    } else if (action == "Cancel") {
        m_confirmingDelete = false;
    }
}

void FilesystemApp::updateContextMenuLayout() {
    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;
    const int minWidth = 140;

    if (m_contextMenuItems.empty() || !m_font) {
        m_contextMenuRect = {0, 0, 0, 0};
        return;
    }

    int maxTextWidth = minWidth;
    for (const auto& item : m_contextMenuItems) {
        int w = 0, h = 0;
        TTF_SizeUTF8(m_font, item.c_str(), &w, &h);
        if (w > maxTextWidth) maxTextWidth = w;
    }

    int menuWidth = maxTextWidth + padding * 2 + 20;
    int menuHeight = static_cast<int>(m_contextMenuItems.size()) * itemHeight + padding * 2;
    int menuX = m_contextMenuPos.x;
    int menuY = m_contextMenuPos.y;

    if (menuX + menuWidth > m_clientWidth) {
        menuX = m_clientWidth - menuWidth - 4;
    }
    if (menuY + menuHeight > m_clientHeight) {
        menuY = m_clientHeight - menuHeight - 4;
    }
    if (menuX < 4) menuX = 4;
    if (menuY < 4) menuY = 4;

    m_contextMenuRect = {menuX, menuY, menuWidth, menuHeight};
}

int FilesystemApp::contextMenuItemAt(int x, int y) const {
    if (!m_showContextMenu || m_contextMenuItems.empty()) {
        return -1;
    }

    SDL_Point point{x, y};
    if (!SDL_PointInRect(&point, &m_contextMenuRect)) {
        return -1;
    }

    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;
    int itemY = m_contextMenuRect.y + padding;

    for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
        SDL_Rect itemRect = {
            m_contextMenuRect.x,
            itemY,
            m_contextMenuRect.w,
            itemHeight
        };
        if (SDL_PointInRect(&point, &itemRect)) {
            return static_cast<int>(i);
        }
        itemY += itemHeight;
    }

    return -1;
}

void FilesystemApp::drawStatusBar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y + contentRect.h - kStatusBarHeight,
        contentRect.w,
        kStatusBarHeight
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

    if (!m_statusMessage.empty()) {
        status += "   |   " + m_statusMessage;
    }

    SDL_Color textCol = {160, 165, 175, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, status.c_str(), textCol);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        if (tex) {
            SDL_Rect dst = {
                contentRect.x + 10,
                bar.y + (kStatusBarHeight - surf->h) / 2,
                std::min(surf->w, contentRect.w - 20),
                surf->h
            };
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void FilesystemApp::setStatus(const std::string& message) {
    m_statusMessage = message;
}

void FilesystemApp::drawContextMenu(SDL_Renderer* r, const SDL_Rect& contentRect) {
    if (!m_showContextMenu || m_contextMenuItems.empty() || !m_font) return;

    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;

    // Convert stored relative menu rect to absolute screen coordinates
    int menuX = contentRect.x + m_contextMenuRect.x;
    int menuY = contentRect.y + m_contextMenuRect.y;

    SDL_Rect menuRect = {menuX, menuY, m_contextMenuRect.w, m_contextMenuRect.h};

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

        SDL_Rect itemRect = {menuX + 1, itemY, m_contextMenuRect.w - 2, itemHeight - 1};

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
