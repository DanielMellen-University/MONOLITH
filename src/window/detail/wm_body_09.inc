
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
    if (!window || window->drawingFilePath.empty()) return;

    auto it = m_fileDrawings.find(window->drawingFilePath);
    if (it != m_fileDrawings.end() && it->second == window) {
        m_fileDrawings.erase(it);
    }
    window->drawingFilePath.clear();
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

    void bindDrawingFile(const std::string& virtualPath) override {
        if (wm && targetWindow) {
            wm->associateDrawingWithFile(targetWindow, virtualPath);
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
