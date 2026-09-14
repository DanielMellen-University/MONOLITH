private:
    std::vector<std::unique_ptr<Window>> m_windows;
    int m_nextId = 0;

    // Currently focused window (top of z-order)
    Window* m_focusedWindow = nullptr;

    // Window being dragged (if any)
    Window* m_draggedWindow = nullptr;

    // Window being resized + direction
    Window* m_resizingWindow = nullptr;
    ResizeDirection m_resizeDirection = ResizeDirection::None;

    // Mouse state
    int m_mouseX = 0;
    int m_mouseY = 0;
    bool m_mouseDown = false;
    // Client window that received the active left-button press, if any.
    // Its release must be delivered even if focus changes or the pointer leaves it.
    Window* m_mouseCaptureWindow = nullptr;
    // Shell controls consume both halves of their left-button interaction.
    bool m_shellMouseCapture = false;

    // Font used for window titles (not owned by WindowManager)
    TTF_Font* m_font = nullptr;

    // Simple cache for title textures: key = window id, value = {texture, last title, was focused}
    struct TitleCacheEntry {
        SDL_Texture* texture = nullptr;
        std::string lastTitle;
        bool wasFocused = false;
    };
    std::unordered_map<int, TitleCacheEntry> m_titleCache;

    // Logical internal desktop size (overridden at startup; matches main.cpp)
    int m_logicalWidth = 1280;
    int m_logicalHeight = 720;

    // Y offset applied before logical<->screen mapping (0 unless host chrome compensation is needed)
    int m_headerOffset = 0;

    // Scale to map logical content into the actual window size
    float m_contentScale = 1.0f;

    // Taskbar button hit testing (populated during render each frame)
    struct TaskbarEntry {
        SDL_Rect rect;
        Window* window;
    };
    std::vector<TaskbarEntry> m_taskbarEntries;

    // Start menu state
    bool m_showStartMenu = false;
    int m_startMenuHoverIndex = -1;
    SDL_Rect m_startMenuRect{0, 0, 0, 0};  // screen coords, updated during render when visible

    // Quit request (from Shut Down etc.)
    bool m_quitRequested = false;

    // Tracks which editor windows are responsible for which files (for singleton-per-file behavior)
    std::unordered_map<std::string, Window*> m_fileEditors;

    std::unordered_map<std::string, Window*> m_fileDrawings;

    std::vector<std::string> m_filesystemClipboardPaths;
    bool m_filesystemClipboardIsCut = false;

    // Tracks active instance numbers per app base type ("Terminal", "Filesystem", etc.)
    // for the dynamic titling system. Populated by claimNextAppInstanceTitle and
    // released in closeWindow (using the annotation stored on each Window).
    std::unordered_map<std::string, std::set<int>> m_activeAppInstances;

    // Resources for creating real apps from the shell (Start Menu launchers)
    TTF_Font* m_appFont = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    monolith::settings::DesktopSettings m_desktopSettings;
    std::string m_desktopSettingsHostPath;

    void applyUiScaleFont();

    // Wallpaper image via WallpaperImage. Cover-scaled over the solid background.
    SDL_Texture* m_wallpaperTexture = nullptr;
    std::string m_wallpaperLoadedPath; // path currently represented by m_wallpaperTexture
    int m_wallpaperTexW = 0;
    int m_wallpaperTexH = 0;

    // Taskbar XP-style horizontal scrolling
    int m_taskbarScrollOffset = 0;        // non-negative logical pixels
    int m_taskbarButtonAreaLeft = 0;      // logical left edge of button strip
    int m_taskbarButtonAreaWidth = 0;     // logical width for buttons
    bool m_taskbarNeedsScroll = false;
    // Screen-space hit targets for scroll arrows (filled during render when scrolling).
    SDL_Rect m_taskbarLeftArrowRect{0, 0, 0, 0};
    SDL_Rect m_taskbarRightArrowRect{0, 0, 0, 0};

    // Taskbar clock (local time). Textures rebuilt when the displayed strings change.
    SDL_Texture* m_clockTexture = nullptr;
    std::string m_clockText;
    int m_clockTexW = 0;
    int m_clockTexH = 0;
    SDL_Rect m_clockHitRect{0, 0, 0, 0};
    SDL_Texture* m_clockDateTexture = nullptr;
    std::string m_clockDateText;
    int m_clockDateTexW = 0;
    int m_clockDateTexH = 0;

    void destroyClockTextures();
    void destroyWallpaperTexture();
    void ensureWallpaperTexture(SDL_Renderer* renderer);
    void renderWallpaper(SDL_Renderer* renderer);

    struct StartMenuItem {
        SDL_Rect rect;  // in screen coordinates
        // 0=Terminal, 1=Text Editor, 2=Filesystem, 3=Settings, 4=Drawing,
        // 5=Snake, 6=Minesweeper, 7=Pong, 8=Breakout, 9=Shut Down
        // Category headers / separators are not added to this list.
        int action;
    };
    std::vector<StartMenuItem> m_startMenuItems;

    struct TitleButtonRects {
        SDL_Rect close;
        SDL_Rect maximize;
        SDL_Rect minimize;
    };

    // Bring a window to the front of the z-order
    void bringToFront(Window* window);

    // Move keyboard focus to the topmost non-minimized survivor, or clear it
    // when every window is minimized.
    void focusTopmostVisibleWindow();

    // Helper to check if a point is inside a window's title bar
    bool isInTitleBar(const Window& window, int x, int y) const;

    SDL_Rect getTaskbarRect() const;
    SDL_Rect getUsableDesktopRect() const;
    int getUsableDesktopBottom() const;
    SDL_Rect logicalRectToScreen(const SDL_Rect& rect) const;
    TitleButtonRects getTitleButtonRects(const Window& window) const;

    // Claims the lowest available positive instance number for the given app base
    // ("Terminal", "Filesystem", "Settings", "Editor" for bare editors, etc.).
    // Reserves it in m_activeAppInstances, computes the display title
    // (bare name for 1, "Base N" for N>1), and returns {title, instanceNumber}.
    // The caller (launchers) must pass the returned values + base to createWindow
    // so the Window is annotated and the slot can be released on close.
    std::pair<std::string, int> claimNextAppInstanceTitle(const std::string& base);

    // After a tracked window of `base` is closed, re-assign contiguous numbers
    // 1..N to all *remaining* live windows of that base (sorted by their previous
    // instance number to preserve relative ordering). Updates their titles and
    // instance numbers, invalidates title caches, and rebuilds the active set.
    // This makes numbers "adjust dynamically" so there are never gaps while
    // windows are open (e.g. closing "Settings" turns the old "Settings 2" into "Settings").
    void compactAppInstances(const std::string& base);

    // After a launcher creates a window, apply restored geometry (session load).
    void applyRestoredGeometry(Window* window, int x, int y, int w, int h,
                               bool minimized, bool maximized);

    bool tryHandleShellHotkeys(const SDL_Event& event);
    void cycleFocus(int direction);
    void endAltTabCycle();

    bool m_altTabCycling = false;
    int m_altTabIndex = 0;
    std::vector<Window*> m_altTabOrder;
};

} // namespace monolith::window
