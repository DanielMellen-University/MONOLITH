            || removal.window->drawingFilePath != removal.boundPath) {
            continue;
        }
        clearDrawingFileBinding(removal.window);
        if (removal.window->app) {
            invokeAppCallback([&]() {
                removal.window->app->onBoundFileRemoved(removal.removedPath);
            });
        }
    }

    forEachLiveApp([&](monolith::app::App& app) {
        app.onVirtualPathRemoved(removed);
    });
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
        if (wm && targetWindow) wm->setWindowTitle(targetWindow, title);
    }

    void restoreTrackedInstanceTitle() override {
        if (!targetWindow) return;
        const std::string title = formatTrackedInstanceTitle(targetWindow);
        if (!title.empty()) {
            if (wm) wm->setWindowTitle(targetWindow, title);
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

    bool focusDrawingForFile(const std::string& virtualPath) override {
        return wm ? wm->focusDrawingForFile(virtualPath) : false;
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

