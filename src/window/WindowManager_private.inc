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
    bool m_hostFocused = true;

    // Font used for window titles (not owned by WindowManager)
    TTF_Font* m_font = nullptr;

    // Per-window title textures store only the UTF-8 prefix that fits the title area.
    struct TitleCacheEntry {
        SDL_Texture* texture = nullptr;
        std::string lastTitle;
        bool wasFocused = false;
        int availableWidth = -1;
    };
    std::unordered_map<int, TitleCacheEntry> m_titleCache;

    struct ShellTextCacheEntry {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        std::size_t estimatedBytes = 0;
        std::list<std::string>::iterator lruPosition;
    };
    static constexpr std::size_t kMaxShellTextCacheEntries = 256;
    static constexpr std::size_t kMaxShellTextCacheBytes = 16 * 1024 * 1024;
    // Shell labels are keyed by text and color and bounded independently of window churn.
    std::unordered_map<std::string, ShellTextCacheEntry> m_shellTextCache;
    std::list<std::string> m_shellTextCacheLru;
    std::size_t m_shellTextCacheBytes = 0;

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
    bool m_taskbarHitTargetsValid = false;

    struct TaskbarLayout {
        SDL_Rect taskbarRect{0, 0, 0, 0};
        int taskbarY = 0;
        int startX = 0;
        int startButtonWidth = 0;
        SDL_Rect startButtonRect{0, 0, 0, 0};
        int buttonHeight = 0;
        int buttonY = 0;
        int buttonsStartX = 0;
        int areaLeftScreen = 0;
        int areaRightScreen = 0;
        SDL_Rect buttonClipRect{0, 0, 0, 0};
        SDL_Rect leftArrowRect{0, 0, 0, 0};
        SDL_Rect rightArrowRect{0, 0, 0, 0};
        bool needsScroll = false;
        std::vector<Window*> buttonOrder;
        std::vector<int> buttonWidths;
    };

    TaskbarLayout computeTaskbarLayout();
    void ensureTaskbarHitTargets();

    // Start menu state
    bool m_showStartMenu = false;
    int m_startMenuHoverIndex = -1;
    int m_startMenuKeyboardIndex = -1;
    SDL_Rect m_startMenuRect{0, 0, 0, 0};  // screen coords, updated during render when visible
    // The app that was suspended when the modal menu opened. Keeping its
    // identity lets a direct handoff skip a duplicate focus-lost callback.
    Window* m_startMenuSuspendedWindow = nullptr;
    int m_startMenuSuspendedWindowId = -1;

    // Quit request (from Shut Down etc.)
    bool m_quitRequested = false;

    // Tracks which editor windows are responsible for which files (for singleton-per-file behavior)
    std::unordered_map<std::string, Window*> m_fileEditors;

    std::unordered_map<std::string, Window*> m_fileDrawings;

    std::vector<std::string> m_filesystemClipboardPaths;
    bool m_filesystemClipboardIsCut = false;

    // Tracks active instance numbers per app base type ("Terminal", "Filesystem", etc.)
    // for the dynamic titling system. Populated by claimNextAppInstanceTitle and
    // released when a tracked window closes or becomes file-backed.
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
    SDL_Rect m_clockTooltipRect{0, 0, 0, 0};
    SDL_Texture* m_clockDateTexture = nullptr;
    std::string m_clockDateText;
    int m_clockDateTexW = 0;
    int m_clockDateTexH = 0;

    void destroyClockTextures();
    void destroyShellTextCache();
    void trimShellTextCacheFor(std::size_t incomingBytes);
    SDL_Texture* getShellTextTexture(SDL_Renderer* renderer, const char* text,
                                      SDL_Color color, int& width, int& height);
    void destroyWallpaperTexture();
    void ensureWallpaperTexture(SDL_Renderer* renderer);
    void renderWallpaper(SDL_Renderer* renderer);

    // Desktop icons (drawn under windows; clickable only on empty desktop).
    int m_desktopIconSelected = -1;
    int m_desktopIconLastClickIndex = -1;
    Uint32 m_desktopIconLastClickTicks = 0;
    struct DesktopIconTextCacheEntry {
        SDL_Texture* glyphTexture = nullptr;
        int glyphW = 0;
        int glyphH = 0;
        SDL_Texture* labelTexture = nullptr;
        int labelW = 0;
        int labelH = 0;
        SDL_Texture* selectedLabelTexture = nullptr;
        int selectedLabelW = 0;
        int selectedLabelH = 0;
    };
    std::array<DesktopIconTextCacheEntry, kDesktopIconCount> m_desktopIconTextCache{};
    void destroyDesktopIconTextCache();
    void renderDesktopIcons(SDL_Renderer* renderer);
    bool tryHandleDesktopIconClick(int logicalX, int logicalY);
    void activateDesktopIcon(DesktopIconAction action);
    void clearDesktopIconSelection();

    struct StartMenuItem {
        SDL_Rect rect;  // in screen coordinates
        // 0=Terminal, 1=Text Editor, 2=Filesystem, 3=Settings, 4=Drawing,
        // 5=Snake, 6=Minesweeper, 7=Pong, 8=Breakout, 9=Shut Down
        // Category headers / separators are not added to this list.
        int action;
    };
    std::vector<StartMenuItem> m_startMenuItems;

    struct StartMenuLayout {
        int menuWidthLogical = 0;
        int headerHLogical = 0;
        int topPadLogical = 0;
        int itemH = 0;
        int itemGap = 0;
        int categoryH = 0;
        int separatorH = 0;
        SDL_Rect menuRect{0, 0, 0, 0};
        int itemStartY = 0;
    };

    StartMenuLayout computeStartMenuLayout(int contentHLogical) const;
    void ensureStartMenuHitTargets();

    struct TitleButtonRects {
        SDL_Rect close;
        SDL_Rect maximize;
        SDL_Rect minimize;
    };

    // Bring a window to the front of the z-order
    void bringToFront(Window* window);

    // App callbacks can close or replace a window while shell code is still
    // handling the original event. Compare identity without dereferencing a
    // possibly stale pointer before continuing the operation.
    bool isLiveWindow(const Window* window) const;
    bool isLiveWindow(const Window* window, int expectedId) const;
    bool isStartMenuSuspended(const Window* window) const;
    void clearStartMenuSuspension();

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

    // These screen-space hit targets are rebuilt during render. Clear them
    // whenever geometry or font metrics change so an event before the next
    // frame cannot use coordinates from the previous layout.
    void invalidateShellHitTargets();

    class AppCallbackScope {
    public:
        explicit AppCallbackScope(WindowManager& owner)
            : m_owner(owner) {
            ++m_owner.m_appCallbackDepth;
        }

        ~AppCallbackScope() {
            --m_owner.m_appCallbackDepth;
            if (m_owner.m_appCallbackDepth == 0) {
                m_owner.processDeferredCloses();
            }
        }

        AppCallbackScope(const AppCallbackScope&) = delete;
        AppCallbackScope& operator=(const AppCallbackScope&) = delete;

    private:
        WindowManager& m_owner;
    };

    template <typename Callback>
    void invokeAppCallback(Callback&& callback) {
        AppCallbackScope scope(*this);
        std::forward<Callback>(callback)();
    }

    void processDeferredCloses();

    // Dispatch window callbacks from a stable snapshot. App callbacks may
    // close or open windows through their controllers, so never iterate
    // m_windows directly while invoking one.
    template <typename Callback>
    void forEachLiveWindow(Callback&& callback) {
        struct Target {
            Window* window;
            int id;
        };

        std::vector<Target> targets;
        targets.reserve(m_windows.size());
        for (const auto& window : m_windows) {
            if (window) targets.push_back({window.get(), window->id});
        }

        for (const Target& target : targets) {
            const auto live = std::find_if(
                m_windows.begin(),
                m_windows.end(),
                [&target](const auto& candidate) {
                    return candidate.get() == target.window
                        && candidate->id == target.id;
                });
            if (live == m_windows.end()) continue;

            Window* window = live->get();
            invokeAppCallback([&]() { callback(*window); });
        }
    }

    template <typename Callback>
    void forEachLiveApp(Callback&& callback) {
        forEachLiveWindow([&callback](Window& window) {
            if (window.app) callback(*window.app);
        });
    }

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

    // Release a bare-app reservation when an untitled Editor or Drawing becomes
    // file-backed, then compact the remaining bare windows.
    void untrackAppInstance(Window* window);

    struct DeferredClose {
        Window* window = nullptr;
        int id = -1;
    };

    int m_appCallbackDepth = 0;
    bool m_processingDeferredCloses = false;
    std::vector<DeferredClose> m_deferredCloses;

    // After a launcher creates a window, apply restored geometry (session load).
    void applyRestoredGeometry(Window* window, int x, int y, int w, int h,
                               bool minimized, bool maximized);

    bool tryHandleShellHotkeys(const SDL_Event& event);
    void setStartMenuVisible(bool visible, bool resumeFocusedApp = true);
    void activateStartMenuItem(int action);
    void cycleFocus(int direction);
    void endAltTabCycle();

    // Key releases paired with shell-consumed keydowns stay out of clients.
    std::set<SDL_Keycode> m_shellKeyUpsPending;
    bool m_altTabCycling = false;
    int m_altTabIndex = 0;
    std::vector<Window*> m_altTabOrder;
};

} // namespace monolith::window
