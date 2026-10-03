void WindowManager::loadDesktopSettings(const std::string& hostPath) {
    m_desktopSettingsHostPath = hostPath;
    m_desktopSettingsSaveFailed = false;
    m_desktopSettings.loadFromHostPath(hostPath);
    if (m_fs && !m_desktopSettings.wallpaperPath().empty()) {
        const std::string rawWallpaperPath = m_desktopSettings.wallpaperPath();
        const std::string normalizedWallpaperPath = m_fs->normalize(rawWallpaperPath);
        if (rawWallpaperPath != normalizedWallpaperPath) {
            m_desktopSettings.setWallpaperPath(normalizedWallpaperPath);
            persistDesktopSettings();
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

bool WindowManager::persistDesktopSettings() {
    if (m_desktopSettingsHostPath.empty()) {
        m_desktopSettingsSaveFailed = false;
        return true;
    }
    const bool saved = m_desktopSettings.saveToHostPath(m_desktopSettingsHostPath);
    m_desktopSettingsSaveFailed = !saved;
    return saved;
}

monolith::app::SettingChangeResult WindowManager::setDesktopBackground(
    uint8_t r, uint8_t g, uint8_t b) {
    m_desktopSettings.setDesktopBackground({r, g, b});
    return persistDesktopSettings()
        ? monolith::app::SettingChangeResult::Applied
        : monolith::app::SettingChangeResult::PersistenceFailed;
}

std::string WindowManager::getWallpaperPath() const {
    return m_desktopSettings.wallpaperPath();
}

monolith::app::SettingChangeResult WindowManager::setWallpaperPath(
    const std::string& virtualPath) {
    using Result = monolith::app::SettingChangeResult;
    if (!monolith::settings::DesktopSettings::isValidWallpaperPath(virtualPath)) {
        return Result::Rejected;
    }
    std::string normalized;
    if (!virtualPath.empty() && m_fs) {
        normalized = m_fs->normalize(virtualPath);
    } else {
        normalized = virtualPath;
    }
    if (normalized == "/") {
        normalized.clear();
    }
    if (!monolith::settings::DesktopSettings::isValidWallpaperPath(normalized)) {
        return Result::Rejected;
    }
    if (m_desktopSettings.wallpaperPath() != normalized) {
        m_desktopSettings.setWallpaperPath(normalized);
        destroyWallpaperTexture();
        m_wallpaperLoadedPath.clear();
    }
    return persistDesktopSettings() ? Result::Applied : Result::PersistenceFailed;
}

std::string WindowManager::getWallpaperFit() const {
    return m_desktopSettings.wallpaperFit();
}

monolith::app::SettingChangeResult WindowManager::setWallpaperFit(const std::string& fit) {
    using Result = monolith::app::SettingChangeResult;
    const std::string next = monolith::settings::DesktopSettings::canonicalizeWallpaperFit(fit);
    if (m_desktopSettings.wallpaperFit() != next) {
        m_desktopSettings.setWallpaperFit(next);
    }
    return persistDesktopSettings() ? Result::Applied : Result::PersistenceFailed;
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

    // cover: fill desktop, crop overflow (default)
    // contain: entire image visible, letterbox/pillarbox over solid bg
    // center: natural 1:1 centered; clip if larger than desktop
    const float scaleX = static_cast<float>(m_logicalWidth) / static_cast<float>(m_wallpaperTexW);
    const float scaleY = static_cast<float>(m_logicalHeight) / static_cast<float>(m_wallpaperTexH);
    const std::string& fit = m_desktopSettings.wallpaperFit();
    float scale = 1.0f;
    if (fit == "contain") {
        scale = scaleX < scaleY ? scaleX : scaleY;
    } else if (fit == "center") {
        scale = 1.0f;
    } else {
        // cover (and any unexpected value)
        scale = scaleX > scaleY ? scaleX : scaleY;
    }

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

    // Clip to the logical desktop so contain/center overflow stays behind windows.
    const RendererClipState previousClip = captureRendererClip(renderer);
    const SDL_Rect desktopClip = {
        logicalToScreenX(0),
        logicalToScreenY(0),
        static_cast<int>(m_logicalWidth * m_contentScale),
        static_cast<int>(m_logicalHeight * m_contentScale)
    };
    const SDL_Rect effectiveClip = intersectRendererClip(desktopClip, previousClip);
    if (effectiveClip.w > 0 && effectiveClip.h > 0) {
        SDL_RenderSetClipRect(renderer, &effectiveClip);
        SDL_RenderCopy(renderer, m_wallpaperTexture, nullptr, &dst);
    }
    restoreRendererClip(renderer, previousClip);
}

bool WindowManager::getClock24Hour() const {
    return m_desktopSettings.clock24Hour();
}

monolith::app::SettingChangeResult WindowManager::setClock24Hour(bool enabled) {
    if (m_desktopSettings.clock24Hour() != enabled) {
        m_desktopSettings.setClock24Hour(enabled);
        // Force the taskbar clock texture to rebuild on the next render.
        if (m_clockTexture) {
            SDL_DestroyTexture(m_clockTexture);
            m_clockTexture = nullptr;
        }
        m_clockText.clear();
        invalidateShellHitTargets();
    }
    return persistDesktopSettings()
        ? monolith::app::SettingChangeResult::Applied
        : monolith::app::SettingChangeResult::PersistenceFailed;
}

int WindowManager::getUiScalePercent() const {
    return m_desktopSettings.uiScalePercent();
}

monolith::app::SettingChangeResult WindowManager::setUiScalePercent(int percent) {
    using Result = monolith::app::SettingChangeResult;
    if (!monolith::settings::DesktopSettings::isSupportedUiScalePercent(percent)) {
        return Result::Rejected;
    }

    if (m_desktopSettings.uiScalePercent() != percent) {
        m_desktopSettings.setUiScalePercent(percent);
        applyUiScaleFont();
        forEachLiveApp([](monolith::app::App& app) {
            app.onUiScaleChanged();
        });
    }
    return persistDesktopSettings() ? Result::Applied : Result::PersistenceFailed;
}
