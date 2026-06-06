#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * A graphical filesystem browser.
 * Allows navigating the internal Monolith filesystem, opening files in the editor,
 * basic creation and deletion.
 */
class FilesystemApp : public App {
public:
    FilesystemApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr);
    ~FilesystemApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    // === Navigation ===
    void setCurrentPath(const std::string& virtualPath);
    void goUp();
    void refreshEntries();
    void activateEntry(size_t index);           // double-click / enter behavior
    std::string fullPathFor(const std::string& name) const;

    // === Actions ===
    void createNewFolder();
    void createNewFile();
    void deleteSelected();
    void startRenameSelected();
    void finishRename(bool commit);  // commit = true for Enter, false for Escape

    // === Selection / Scrolling ===
    void setSelection(int index);
    bool selectEntryNamed(const std::string& name, bool isDirectory);
    void clampSelection();
    void ensureSelectionVisible();
    int getVisibleRowCount(const SDL_Rect& contentRect) const;

    // === Input helpers ===
    void handleMouseButton(const SDL_MouseButtonEvent& e);
    void handleMouseWheel(const SDL_MouseWheelEvent& e);
    void handleKeyDown(const SDL_Keysym& keysym);

    // === Rendering helpers ===
    int getRowHeight() const;
    void drawPathBar(SDL_Renderer* r, const SDL_Rect& contentRect, int& outTopY);
    void drawList(SDL_Renderer* r, const SDL_Rect& contentRect, int topY);
    void drawToolbar(SDL_Renderer* r, const SDL_Rect& contentRect);
    void drawStatusBar(SDL_Renderer* r, const SDL_Rect& contentRect);
    void drawContextMenu(SDL_Renderer* r, const SDL_Rect& contentRect);

    void showContextMenu(int x, int y, int targetIndex);
    void closeContextMenu();
    void executeContextMenuAction(int menuIndex);
    void updateContextMenuLayout();
    int contextMenuItemAt(int x, int y) const;
    void setStatus(const std::string& message);

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::string m_currentPath = "/home/monolith";
    std::vector<monolith::fs::Filesystem::DirEntry> m_entries;

    int m_selectedIndex = -1;   // -1 means nothing selected
    int m_scrollOffset = 0;     // first visible row index

    // Cached layout
    int m_clientWidth = 0;
    int m_clientHeight = 0;

    // Simple toolbar button hit areas (updated each render)
    SDL_Rect m_btnUp{0,0,0,0};
    SDL_Rect m_btnNewFolder{0,0,0,0};
    SDL_Rect m_btnNewFile{0,0,0,0};
    SDL_Rect m_btnDelete{0,0,0,0};
    SDL_Rect m_btnRename{0,0,0,0};

    // Rename state
    bool m_renaming = false;
    int m_renameIndex = -1;
    std::string m_renameBuffer;

    // Delete confirmation state
    bool m_confirmingDelete = false;

    // Context menu state
    bool m_showContextMenu = false;
    SDL_Point m_contextMenuPos{0, 0};
    SDL_Rect m_contextMenuRect{0, 0, 0, 0};
    int m_contextMenuTarget = -1;          // index in m_entries, or -1 for background
    std::vector<std::string> m_contextMenuItems;
    int m_contextMenuHoverIndex = -1;

    std::string m_statusMessage;

    // For double-click detection we use SDL's built-in clicks count
};

} // namespace monolith::app
