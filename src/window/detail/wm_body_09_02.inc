    forEachLiveApp([&](monolith::app::App& app) {
        app.onVirtualPathMoved(oldNormalized, newNormalized);
    });
}

void WindowManager::notifyVirtualPathCreated(const std::string& virtualPath) {
    if (!m_fs) return;

    const std::string created = m_fs->normalize(virtualPath);
    if (created.empty() || created == "/") return;

    // A configured wallpaper may have been missing when the previous render
    // attempted to load it. Clear that failed-path cache when the file itself
    // or one of its newly-created parent directories appears.
    const std::string wallpaper = m_fs->normalize(m_desktopSettings.wallpaperPath());
    if (!wallpaper.empty() && m_fs->isSameOrDescendant(created, wallpaper)) {
        destroyWallpaperTexture();
        m_wallpaperLoadedPath.clear();
    }

    forEachLiveApp([&](monolith::app::App& app) {
        app.onVirtualPathCreated(created);
    });
}

void WindowManager::notifyVirtualPathChanged(const std::string& virtualPath) {
    if (!m_fs) return;

    const std::string changed = m_fs->normalize(virtualPath);
    if (changed.empty() || changed == "/") return;

    const std::string wallpaper = m_fs->normalize(m_desktopSettings.wallpaperPath());
    if (!wallpaper.empty() && m_fs->isSameOrDescendant(changed, wallpaper)) {
        destroyWallpaperTexture();
        m_wallpaperLoadedPath.clear();
    }

    forEachLiveApp([&](monolith::app::App& app) {
        app.onVirtualPathChanged(changed);
    });
}

void WindowManager::notifyVirtualPathRemoved(const std::string& virtualPath) {
    if (!m_fs) return;

    const std::string removed = m_fs->normalize(virtualPath);
    if (removed == "/") return;

    if (m_fs->isSameOrDescendant(removed, m_desktopSettings.wallpaperPath())) {
        m_desktopSettings.setWallpaperPath("");
        destroyWallpaperTexture();
        m_wallpaperLoadedPath.clear();
        if (!m_desktopSettingsHostPath.empty()) {
            m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
        }
    }

    std::vector<std::string> remainingClipboardPaths;
    remainingClipboardPaths.reserve(m_filesystemClipboardPaths.size());
    for (const auto& path : m_filesystemClipboardPaths) {
        if (!m_fs->isSameOrDescendant(removed, path)) {
            remainingClipboardPaths.push_back(path);
        }
    }
    if (remainingClipboardPaths.size() != m_filesystemClipboardPaths.size()) {
        m_filesystemClipboardPaths = std::move(remainingClipboardPaths);
        if (m_filesystemClipboardPaths.empty()) {
            m_filesystemClipboardIsCut = false;
        }
    }

    struct BindingRemoval {
        Window* window;
        int id;
        std::string boundPath;
        std::string removedPath;
    };
    std::vector<BindingRemoval> removedEditors;
    std::vector<BindingRemoval> removedDrawings;
    for (const auto& windowPtr : m_windows) {
        Window* window = windowPtr.get();
        if (!window) continue;
        if (!window->editedFilePath.empty()
            && m_fs->isSameOrDescendant(removed, window->editedFilePath)) {
            removedEditors.push_back({window, window->id, window->editedFilePath, removed});
        }
        if (!window->drawingFilePath.empty()
            && m_fs->isSameOrDescendant(removed, window->drawingFilePath)) {
            removedDrawings.push_back({window, window->id, window->drawingFilePath, removed});
        }
    }

    for (const BindingRemoval& removal : removedEditors) {
        if (!isLiveWindow(removal.window, removal.id)
            || removal.window->editedFilePath != removal.boundPath) {
            continue;
        }
        clearEditorFileBinding(removal.window);
        if (removal.window->app) {
            invokeAppCallback([&]() {
                removal.window->app->onBoundFileRemoved(removal.removedPath);
            });
        }
    }
    for (const BindingRemoval& removal : removedDrawings) {
        if (!isLiveWindow(removal.window, removal.id)
