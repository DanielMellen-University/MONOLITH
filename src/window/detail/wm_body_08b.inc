void WindowManager::loadDesktopSettings(const std::string& hostPath) {
    m_desktopSettingsHostPath = hostPath;
    m_desktopSettings.loadFromHostPath(hostPath);
    if (m_fs && !m_desktopSettings.wallpaperPath().empty()) {
        const std::string rawWallpaperPath = m_desktopSettings.wallpaperPath();
        const std::string normalizedWallpaperPath = m_fs->normalize(rawWallpaperPath);
        if (rawWallpaperPath != normalizedWallpaperPath) {
            m_desktopSettings.setWallpaperPath(normalizedWallpaperPath);
            m_desktopSettings.saveToHostPath(hostPath);
        }
    }
    applyUiScaleFont();
    // Force wallpaper reload on next render (path may have changed).
    destroyWallpaperTexture();
    m_wallpaperLoadedPath.clear();
}

monolith::settings::RGB WindowManager::getDesktopBackground() const {
    return m_desktopSettings.desktopBackground();
}

void WindowManager::setDesktopBackground(uint8_t r, uint8_t g, uint8_t b) {
    m_desktopSettings.setDesktopBackground({r, g, b});
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

std::string WindowManager::getWallpaperPath() const {
    return m_desktopSettings.wallpaperPath();
}

void WindowManager::setWallpaperPath(const std::string& virtualPath) {
    std::string normalized;
    if (!virtualPath.empty() && m_fs) {
        normalized = m_fs->normalize(virtualPath);
    } else {
        normalized = virtualPath;
    }
    if (normalized == "/") {
        normalized.clear();
    }
    if (m_desktopSettings.wallpaperPath() == normalized) {
        return;
    }
    m_desktopSettings.setWallpaperPath(normalized);
    destroyWallpaperTexture();
    m_wallpaperLoadedPath.clear();
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

void WindowManager::destroyWallpaperTexture() {
    if (m_wallpaperTexture) {
        SDL_DestroyTexture(m_wallpaperTexture);
        m_wallpaperTexture = nullptr;
    }
    m_wallpaperTexW = 0;
    m_wallpaperTexH = 0;
}

void WindowManager::ensureWallpaperTexture(SDL_Renderer* renderer) {
    const std::string& path = m_desktopSettings.wallpaperPath();
    if (path.empty()) {
        if (m_wallpaperTexture) {
            destroyWallpaperTexture();
            m_wallpaperLoadedPath.clear();
        }
        return;
    }
    if (m_wallpaperTexture && m_wallpaperLoadedPath == path) {
        return;
    }

    destroyWallpaperTexture();
    m_wallpaperLoadedPath = path;

    if (!renderer || !m_fs) {
        return;
    }
    if (!m_fs->isFile(path)) {
        return;
    }

    const std::string hostPath = m_fs->toHostPath(path);
    SDL_Surface* surface = loadWallpaperSurface(hostPath);
    if (!surface) {
        // Fail soft: keep solid color background.
        return;
    }

    m_wallpaperTexture = SDL_CreateTextureFromSurface(renderer, surface);
    m_wallpaperTexW = surface->w;
    m_wallpaperTexH = surface->h;
    SDL_FreeSurface(surface);
}

void WindowManager::renderWallpaper(SDL_Renderer* renderer) {
    if (!renderer) return;
    ensureWallpaperTexture(renderer);
    if (!m_wallpaperTexture || m_wallpaperTexW <= 0 || m_wallpaperTexH <= 0) {
        return;
    }

    // Cover-scale: fill the logical desktop, cropping overflow, centered.
    const float scaleX = static_cast<float>(m_logicalWidth) / static_cast<float>(m_wallpaperTexW);
    const float scaleY = static_cast<float>(m_logicalHeight) / static_cast<float>(m_wallpaperTexH);
    const float scale = scaleX > scaleY ? scaleX : scaleY;

    const int drawW = static_cast<int>(m_wallpaperTexW * scale);
    const int drawH = static_cast<int>(m_wallpaperTexH * scale);
    const int logicalX = (m_logicalWidth - drawW) / 2;
    const int logicalY = (m_logicalHeight - drawH) / 2;

    SDL_Rect dst = {
        logicalToScreenX(logicalX),
        logicalToScreenY(logicalY),
        static_cast<int>(drawW * m_contentScale),
        static_cast<int>(drawH * m_contentScale)
    };
    SDL_RenderCopy(renderer, m_wallpaperTexture, nullptr, &dst);
}

bool WindowManager::getClock24Hour() const {
    return m_desktopSettings.clock24Hour();
}

void WindowManager::setClock24Hour(bool enabled) {
    if (m_desktopSettings.clock24Hour() == enabled) {
        return;
    }
    m_desktopSettings.setClock24Hour(enabled);
    // Force the taskbar clock texture to rebuild on the next render.
    if (m_clockTexture) {
        SDL_DestroyTexture(m_clockTexture);
        m_clockTexture = nullptr;
    }
    m_clockText.clear();
    invalidateShellHitTargets();
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}

int WindowManager::getUiScalePercent() const {
    return m_desktopSettings.uiScalePercent();
}

void WindowManager::setUiScalePercent(int percent) {
    if (!monolith::settings::DesktopSettings::isSupportedUiScalePercent(percent)) return;
    if (m_desktopSettings.uiScalePercent() == percent) return;

    m_desktopSettings.setUiScalePercent(percent);
    applyUiScaleFont();
    forEachLiveApp([](monolith::app::App& app) {
        app.onUiScaleChanged();
    });
    if (!m_desktopSettingsHostPath.empty()) {
        m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    }
}
