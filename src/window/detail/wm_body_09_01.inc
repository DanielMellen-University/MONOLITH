    if (!m_fs) return;

    const std::string oldNormalized = m_fs->normalize(oldPath);
    const std::string newNormalized = m_fs->normalize(newPath);
    if (oldNormalized == newNormalized || oldNormalized == "/") return;

    auto remap = [&](const std::string& current) -> std::string {
        const std::string normalizedCurrent = m_fs->normalize(current);
        if (!m_fs->isSameOrDescendant(oldNormalized, normalizedCurrent)) return {};
        return newNormalized + normalizedCurrent.substr(oldNormalized.size());
    };

    const std::string nextWallpaperPath = remap(m_desktopSettings.wallpaperPath());
    if (!nextWallpaperPath.empty()) {
        m_desktopSettings.setWallpaperPath(nextWallpaperPath);
        destroyWallpaperTexture();
        m_wallpaperLoadedPath.clear();
        if (!m_desktopSettingsHostPath.empty()) {
            m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
        }
    }

    std::vector<std::string> remappedClipboardPaths;
    remappedClipboardPaths.reserve(m_filesystemClipboardPaths.size());
    bool clipboardChanged = false;
    for (const auto& path : m_filesystemClipboardPaths) {
        const std::string next = remap(path);
        if (next.empty()) {
            remappedClipboardPaths.push_back(path);
        } else {
            remappedClipboardPaths.push_back(next);
            clipboardChanged = true;
        }
    }
    if (clipboardChanged) {
        m_filesystemClipboardPaths = std::move(remappedClipboardPaths);
    }

    struct BindingMove {
        Window* window;
        int id;
        std::string previous;
        std::string next;
    };
    std::vector<BindingMove> editorUpdates;
    std::vector<BindingMove> drawingUpdates;
    for (const auto& windowPtr : m_windows) {
        Window* window = windowPtr.get();
        if (!window) continue;
        if (!window->editedFilePath.empty()) {
            const std::string previous = window->editedFilePath;
            const std::string next = remap(previous);
            if (!next.empty()) editorUpdates.push_back({window, window->id, previous, next});
        }
        if (!window->drawingFilePath.empty()) {
            const std::string previous = window->drawingFilePath;
            const std::string next = remap(previous);
            if (!next.empty()) drawingUpdates.push_back({window, window->id, previous, next});
        }
    }

    for (const BindingMove& update : editorUpdates) {
        if (!isLiveWindow(update.window, update.id)
            || update.window->editedFilePath != update.previous) {
            continue;
        }
        Window* window = update.window;
        auto current = m_fileEditors.find(update.previous);
        if (current != m_fileEditors.end() && current->second != window) continue;
        if (current != m_fileEditors.end() && current->second == window) {
            m_fileEditors.erase(current);
        }
        window->editedFilePath = update.next;
        m_fileEditors[update.next] = window;
        if (window->app) {
            invokeAppCallback([&]() {
                window->app->onBoundFileMoved(update.previous, update.next);
            });
        }
    }

    for (const BindingMove& update : drawingUpdates) {
        if (!isLiveWindow(update.window, update.id)
            || update.window->drawingFilePath != update.previous) {
            continue;
        }
        Window* window = update.window;
        auto current = m_fileDrawings.find(update.previous);
        if (current != m_fileDrawings.end() && current->second != window) continue;
        if (current != m_fileDrawings.end() && current->second == window) {
            m_fileDrawings.erase(current);
        }
        window->drawingFilePath = update.next;
        m_fileDrawings[update.next] = window;
        if (window->app) {
            invokeAppCallback([&]() {
                window->app->onBoundFileMoved(update.previous, update.next);
            });
        }
    }

