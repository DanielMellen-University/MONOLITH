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

    monolith::app::SettingChangeResult setDesktopBackgroundColor(
        uint8_t r, uint8_t g, uint8_t b) override {
        if (wm) {
            return wm->setDesktopBackground(r, g, b);
        }
        return monolith::app::SettingChangeResult::Rejected;
    }

    bool getClock24Hour() const override {
        return wm ? wm->getClock24Hour() : false;
    }

    monolith::app::SettingChangeResult setClock24Hour(bool enabled) override {
        if (wm) {
            return wm->setClock24Hour(enabled);
        }
        return monolith::app::SettingChangeResult::Rejected;
    }

    int getUiScalePercent() const override {
        return wm ? wm->getUiScalePercent() : 100;
    }

    monolith::app::SettingChangeResult setUiScalePercent(int percent) override {
        if (wm) {
            return wm->setUiScalePercent(percent);
        }
        return monolith::app::SettingChangeResult::Rejected;
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

    monolith::app::SettingChangeResult setWallpaperPath(
        const std::string& virtualPath) override {
        if (wm) {
            return wm->setWallpaperPath(virtualPath);
        }
        return monolith::app::SettingChangeResult::Rejected;
    }

    std::string getWallpaperFit() const override {
        return wm ? wm->getWallpaperFit() : std::string{"cover"};
    }

    monolith::app::SettingChangeResult setWallpaperFit(const std::string& fit) override {
        if (wm) {
            return wm->setWallpaperFit(fit);
        }
        return monolith::app::SettingChangeResult::Rejected;
    }

    bool desktopSettingsSaveFailed() const override {
        return wm && wm->desktopSettingsSaveFailed();
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
