#include "FilesystemApp.hpp"

#include <algorithm>
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

    // Leave some space for path bar + toolbar area at top and a little padding
    const int reservedTop = 52;   // path bar + small toolbar strip
    const int reservedBottom = 8;
    int available = contentRect.h - reservedTop - reservedBottom;
    if (available < 20) available = 20;

    return std::max(3, available / rh);
}

void FilesystemApp::handleMouseButton(const SDL_MouseButtonEvent& e, const SDL_Rect& contentRect) {
    if (e.button != SDL_BUTTON_LEFT) return;

    const int mx = e.x;
    const int my = e.y;

    // Check toolbar buttons first (they live near the top of contentRect)
    SDL_Point pt { mx, my };
    if (SDL_PointInRect(&pt, &m_btnUp)) {
        goUp();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnNewFolder)) {
        createNewFolder();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnDelete)) {
        deleteSelected();
        return;
    }

    // Path bar area is above the list — ignore clicks there for selection
    const int listTop = contentRect.y + 52; // must match drawPathBar + toolbar height
    if (my < listTop) return;

    // Compute which row was clicked
    int rowHeight = getRowHeight();
    if (rowHeight <= 0) rowHeight = 20;

    int relY = my - listTop;
    if (relY < 0) return;

    int clickedRow = m_scrollOffset + (relY / rowHeight);
    if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
        setSelection(clickedRow);

        // Double-click?
        if (e.clicks >= 2) {
            activateEntry(static_cast<size_t>(clickedRow));
        }
    } else {
        // Clicked empty space
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

        case SDLK_F5:
            refreshEntries();
            break;

        default:
            break;
    }
}

void FilesystemApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        // We need the current content rect to do hit testing.
        // Since we don't store it, we do a best-effort using cached client size.
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
    const int y = contentRect.y + 30;
    const int h = 20;
    const int padding = 8;
    const int gap = 6;

    int x = contentRect.x + padding;

    auto drawButton = [&](SDL_Rect& outRect, const char* label, int w) {
        outRect = {x, y, w, h};
        x += w + gap;

        // Button background
        SDL_SetRenderDrawColor(r, 45, 48, 55, 255);
        SDL_RenderFillRect(r, &outRect);

        // Subtle border
        SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
        SDL_RenderDrawRect(r, &outRect);

        if (m_font) {
            SDL_Color col = {210, 215, 220, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, label, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_Rect dst = {
                        outRect.x + (outRect.w - surf->w) / 2,
                        outRect.y + (outRect.h - surf->h) / 2,
                        surf->w, surf->h
                    };
                    SDL_RenderCopy(r, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
    };

    drawButton(m_btnUp, "Up", 48);
    drawButton(m_btnNewFolder, "New Folder", 92);
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
    const int listHeight = contentRect.h - (listTopY - contentRect.y) - 6;

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

        SDL_Rect rowRect = {
            listArea.x,
            y,
            listArea.w,
            rowH - 1
        };

        if (isSelected) {
            SDL_SetRenderDrawColor(r, selBg.r, selBg.g, selBg.b, 255);
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

        // Name
        SDL_Color nameCol = isSelected ? selText : (entry.isDirectory ? textDir : textNormal);
        {
            SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, entry.name.c_str(), nameCol);
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
        }

        // (Trailing slash for directories intentionally omitted for v1 to keep layout simple)

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
}

void FilesystemApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    ensureSelectionVisible();
}

} // namespace monolith::app
