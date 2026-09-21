
void WindowManager::associateEditorWithFile(Window* window, const std::string& virtualPath) {
    if (!window || virtualPath.empty() || !m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);

    auto existing = m_fileEditors.find(normalized);
    if (existing != m_fileEditors.end() && existing->second && existing->second != window) {
        bringToFront(existing->second);
        return;
    }

    // A bare editor can become file-backed through Save As. File-backed
    // editors are intentionally outside the bare Editor instance pool.
    untrackAppInstance(window);

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
        setWindowTitle(window, title);
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

    // A bare sketch can become file-backed through Save. File-backed
    // Drawings are intentionally outside the bare Drawing instance pool.
    untrackAppInstance(window);

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
        setWindowTitle(window, title);
    }
}

void WindowManager::notifyVirtualPathMoved(const std::string& oldPath,
                                           const std::string& newPath) {
