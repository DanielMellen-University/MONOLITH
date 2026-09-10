
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

void WindowManager::clearEditorFileBinding(Window* window) {
    if (!window) return;

    if (!window->editedFilePath.empty()) {
        auto it = m_fileEditors.find(window->editedFilePath);
        if (it != m_fileEditors.end() && it->second == window) {
            m_fileEditors.erase(it);
        }
        window->editedFilePath.clear();
    }

    if (window->appBaseTitle.empty()) {
        auto [title, instance] = claimNextAppInstanceTitle("Editor");
        window->appBaseTitle = "Editor";
        window->appInstanceNumber = instance;
        window->title = title;
    }
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
    if (!window) return;

    const bool wasBound = !window->drawingFilePath.empty();
    if (wasBound) {
        auto it = m_fileDrawings.find(window->drawingFilePath);
        if (it != m_fileDrawings.end() && it->second == window) {
            m_fileDrawings.erase(it);
        }
        window->drawingFilePath.clear();
    }

    if (wasBound && window->appBaseTitle.empty()) {
        auto [title, instance] = claimNextAppInstanceTitle("Drawing");
        window->appBaseTitle = "Drawing";
        window->appInstanceNumber = instance;
        window->title = title;
    }
}

void WindowManager::notifyVirtualPathMoved(const std::string& oldPath,
                                           const std::string& newPath) {
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

    std::vector<std::pair<Window*, std::string>> editorUpdates;
    std::vector<std::pair<Window*, std::string>> drawingUpdates;
    for (const auto& windowPtr : m_windows) {
        Window* window = windowPtr.get();
        if (!window) continue;
        if (!window->editedFilePath.empty()) {
            const std::string next = remap(window->editedFilePath);
            if (!next.empty()) editorUpdates.emplace_back(window, next);
        }
        if (!window->drawingFilePath.empty()) {
            const std::string next = remap(window->drawingFilePath);
            if (!next.empty()) drawingUpdates.emplace_back(window, next);
        }
    }

    for (const auto& [window, next] : editorUpdates) {
        const std::string previous = window->editedFilePath;
        auto current = m_fileEditors.find(window->editedFilePath);
        if (current != m_fileEditors.end() && current->second == window) {
            m_fileEditors.erase(current);
        }
        window->editedFilePath = next;
        m_fileEditors[next] = window;
        if (window->app) window->app->onBoundFileMoved(previous, next);
    }

    for (const auto& [window, next] : drawingUpdates) {
        const std::string previous = window->drawingFilePath;
        auto current = m_fileDrawings.find(window->drawingFilePath);
        if (current != m_fileDrawings.end() && current->second == window) {
            m_fileDrawings.erase(current);
        }
        window->drawingFilePath = next;
        m_fileDrawings[next] = window;
        if (window->app) window->app->onBoundFileMoved(previous, next);
    }

    for (const auto& windowPtr : m_windows) {
        if (windowPtr && windowPtr->app) {
            windowPtr->app->onVirtualPathMoved(oldNormalized, newNormalized);
        }
    }
}

void WindowManager::notifyVirtualPathCreated(const std::string& virtualPath) {
    if (!m_fs) return;

    const std::string created = m_fs->normalize(virtualPath);
    if (created.empty() || created == "/") return;

    for (const auto& windowPtr : m_windows) {
        if (windowPtr && windowPtr->app) {
            windowPtr->app->onVirtualPathCreated(created);
        }
    }
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

    for (const auto& windowPtr : m_windows) {
        if (windowPtr && windowPtr->app) {
            windowPtr->app->onVirtualPathChanged(changed);
        }
    }
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

    std::vector<std::pair<Window*, std::string>> removedEditors;
    std::vector<std::pair<Window*, std::string>> removedDrawings;
    for (const auto& windowPtr : m_windows) {
        Window* window = windowPtr.get();
        if (!window) continue;
        if (!window->editedFilePath.empty()
            && m_fs->isSameOrDescendant(removed, window->editedFilePath)) {
            removedEditors.emplace_back(window, window->editedFilePath);
        }
        if (!window->drawingFilePath.empty()
            && m_fs->isSameOrDescendant(removed, window->drawingFilePath)) {
            removedDrawings.emplace_back(window, window->drawingFilePath);
        }
    }

    for (const auto& [window, removedPath] : removedEditors) {
        clearEditorFileBinding(window);
        if (window->app) window->app->onBoundFileRemoved(removedPath);
    }
    for (const auto& [window, removedPath] : removedDrawings) {
        clearDrawingFileBinding(window);
        if (window->app) window->app->onBoundFileRemoved(removedPath);
    }

    for (const auto& windowPtr : m_windows) {
        if (windowPtr && windowPtr->app) {
            windowPtr->app->onVirtualPathRemoved(removed);
        }
    }
}

bool WindowManager::getFilesystemClipboard(std::vector<std::string>& paths, bool& isCut) const {
    paths = m_filesystemClipboardPaths;
    isCut = m_filesystemClipboardIsCut;
    return !paths.empty();
}

void WindowManager::setFilesystemClipboard(const std::vector<std::string>& paths, bool isCut) {
    m_filesystemClipboardPaths = paths;
    m_filesystemClipboardIsCut = isCut && !paths.empty();
}

void WindowManager::clearFilesystemClipboard() {
    m_filesystemClipboardPaths.clear();
    m_filesystemClipboardIsCut = false;
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

    void openPath(const std::string& virtualPath) override {
        if (wm) {
            wm->openPath(virtualPath);
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

    void clearEditorFileBinding() override {
        if (wm && targetWindow) {
            wm->clearEditorFileBinding(targetWindow);
        }
    }

    void bindDrawingFile(const std::string& virtualPath) override {
        if (wm && targetWindow) {
            wm->associateDrawingWithFile(targetWindow, virtualPath);
        }
    }

    void notifyVirtualPathMoved(const std::string& oldPath,
                                const std::string& newPath) override {
        if (wm) {
            wm->notifyVirtualPathMoved(oldPath, newPath);
        }
    }

    void notifyVirtualPathCreated(const std::string& virtualPath) override {
        if (wm) {
            wm->notifyVirtualPathCreated(virtualPath);
        }
    }

    void notifyVirtualPathChanged(const std::string& virtualPath) override {
        if (wm) {
            wm->notifyVirtualPathChanged(virtualPath);
        }
    }

    void notifyVirtualPathRemoved(const std::string& virtualPath) override {
        if (wm) {
            wm->notifyVirtualPathRemoved(virtualPath);
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

    bool getClock24Hour() const override {
        return wm ? wm->getClock24Hour() : false;
    }

    void setClock24Hour(bool enabled) override {
        if (wm) {
            wm->setClock24Hour(enabled);
        }
    }

    int getUiScalePercent() const override {
        return wm ? wm->getUiScalePercent() : 100;
    }

    void setUiScalePercent(int percent) override {
        if (wm) {
            wm->setUiScalePercent(percent);
        }
    }

    void getLogicalDesktopSize(int& width, int& height) const override {
        if (wm) {
            wm->getLogicalDesktopSize(width, height);
            return;
        }
        width = 1280;
        height = 720;
    }

    bool getFilesystemClipboard(std::vector<std::string>& paths, bool& isCut) const override {
        return wm ? wm->getFilesystemClipboard(paths, isCut) : false;
    }

    void setFilesystemClipboard(const std::vector<std::string>& paths, bool isCut) override {
        if (wm) {
            wm->setFilesystemClipboard(paths, isCut);
        }
    }

    void clearFilesystemClipboard() override {
        if (wm) {
            wm->clearFilesystemClipboard();
        }
    }

    std::string getWallpaperPath() const override {
        return wm ? wm->getWallpaperPath() : std::string{};
    }

    void setWallpaperPath(const std::string& virtualPath) override {
        if (wm) {
            wm->setWallpaperPath(virtualPath);
        }
    }
};

} // anonymous namespace

monolith::app::IWindowController* WindowManager::createControllerFor(Window* window) {
    // Own the controller on the Window so it is destroyed with that window
    // (no process-global static list / dangling targetWindow after close).
    if (!window) return nullptr;

    auto ctrl = std::make_unique<WindowController>();
    ctrl->wm = this;
    ctrl->targetWindow = window;

    monolith::app::IWindowController* raw = ctrl.get();
    window->controller = std::move(ctrl);
    return raw;
}

} // namespace monolith::window
