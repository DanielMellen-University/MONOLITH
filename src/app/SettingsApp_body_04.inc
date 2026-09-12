        m_wallpaperCursorPos = m_wallpaperEditBuffer.size();
        m_wallpaperScrollPx = 0;
        return;
    }

    // Click elsewhere in the panel drops wallpaper focus.
    if (m_wallpaperFieldFocused) {
        m_wallpaperFieldFocused = false;
        syncWallpaperBufferFromShell();
    }

    for (int i = 0; i < 2; ++i) {
        if (pointInRect(x, y, m_clockFormatHitRects[static_cast<size_t>(i)])) {
            applyClock24Hour(i == 1);
            return;
        }
    }

    for (int i = 0; i < kUiScaleCount; ++i) {
        if (pointInRect(x, y, m_uiScaleHitRects[static_cast<size_t>(i)])) {
            applyUiScale(kUiScaleOptions[static_cast<size_t>(i)].percent);
            return;
        }
    }
}

void SettingsApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    buildInfoLines();
    clampScrollOffset();
}

