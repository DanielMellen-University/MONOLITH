void DrawingApp::endActiveStroke() {
    m_drawing = false;
    m_lastCanvasX = -1;
    m_lastCanvasY = -1;
    m_shapeAnchorX = -1;
    m_shapeAnchorY = -1;
    finishSparseHistory();
}

void DrawingApp::refreshDirtyState() {
    rebuildDirtyTiles();
}

void DrawingApp::rebuildDirtyTiles() {
    const size_t columns = m_canvasWidth > 0
        ? (static_cast<size_t>(m_canvasWidth - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t rows = m_canvasHeight > 0
        ? (static_cast<size_t>(m_canvasHeight - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t tileCount = rows != 0
        && columns > std::numeric_limits<size_t>::max() / rows
        ? 0
        : columns * rows;
    m_dirtyTileWidth = m_canvasWidth;
    m_dirtyTileHeight = m_canvasHeight;
    m_dirtyTileColumns = columns;
    m_dirtyTiles.assign(tileCount, 0);
    m_dirtyTileCount = 0;

    if (m_canvasWidth != m_savedSnapshot.width
        || m_canvasHeight != m_savedSnapshot.height
        || m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        markAllDirtyTiles();
        return;
    }

    const size_t width = static_cast<size_t>(m_canvasWidth);
    const size_t height = static_cast<size_t>(m_canvasHeight);
    if (width > std::numeric_limits<size_t>::max() / height / 4
        || m_pixels.size() != width * height * 4
        || m_savedSnapshot.pixels.size() != m_pixels.size()) {
        markAllDirtyTiles();
        return;
    }

    for (size_t tileY = 0; tileY < rows; ++tileY) {
        const int y = static_cast<int>(tileY * kHistoryTileSize);
        const int tileHeight = std::min(kHistoryTileSize, m_canvasHeight - y);
        for (size_t tileX = 0; tileX < columns; ++tileX) {
            const int x = static_cast<int>(tileX * kHistoryTileSize);
            const int tileWidth = std::min(kHistoryTileSize, m_canvasWidth - x);
            const size_t rowBytes = static_cast<size_t>(tileWidth) * 4;
            bool differs = false;
            for (int row = 0; row < tileHeight && !differs; ++row) {
                const size_t offset =
                    (static_cast<size_t>(y + row) * width + static_cast<size_t>(x)) * 4;
                differs = std::memcmp(m_pixels.data() + offset,
                                      m_savedSnapshot.pixels.data() + offset,
                                      rowBytes) != 0;
            }
            if (differs) {
                const size_t index = tileY * columns + tileX;
                m_dirtyTiles[index] = 1;
                ++m_dirtyTileCount;
            }
        }
    }
    updateDirtyFlag();
}

void DrawingApp::clearDirtyTiles() {
    const size_t columns = m_canvasWidth > 0
        ? (static_cast<size_t>(m_canvasWidth - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t rows = m_canvasHeight > 0
        ? (static_cast<size_t>(m_canvasHeight - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t tileCount = rows != 0
        && columns > std::numeric_limits<size_t>::max() / rows
        ? 0
        : columns * rows;
    m_dirtyTileWidth = m_canvasWidth;
    m_dirtyTileHeight = m_canvasHeight;
    m_dirtyTileColumns = columns;
    m_dirtyTiles.assign(tileCount, 0);
    m_dirtyTileCount = 0;
    updateDirtyFlag();
}

void DrawingApp::markAllDirtyTiles() {
    if (m_dirtyTileWidth != m_canvasWidth || m_dirtyTileHeight != m_canvasHeight) {
        clearDirtyTiles();
    }
    std::fill(m_dirtyTiles.begin(), m_dirtyTiles.end(), 1);
    m_dirtyTileCount = m_dirtyTiles.size();
    updateDirtyFlag();
}

void DrawingApp::markDirtyTile(size_t index) {
    if (m_dirtyTileWidth != m_canvasWidth || m_dirtyTileHeight != m_canvasHeight) {
        rebuildDirtyTiles();
    }
    if (index >= m_dirtyTiles.size()) return;
    if (!m_dirtyTiles[index]) {
        m_dirtyTiles[index] = 1;
        ++m_dirtyTileCount;
    }
    updateDirtyFlag();
}

void DrawingApp::refreshDirtyTile(const CanvasTileSnapshot& tile) {
    if (m_canvasWidth != m_savedSnapshot.width
        || m_canvasHeight != m_savedSnapshot.height
        || m_dirtyTileWidth != m_canvasWidth
        || m_dirtyTileHeight != m_canvasHeight) {
        rebuildDirtyTiles();
        return;
    }

    const size_t tileX = static_cast<size_t>(tile.x) / kHistoryTileSize;
    const size_t tileY = static_cast<size_t>(tile.y) / kHistoryTileSize;
    const size_t index = tileY * m_dirtyTileColumns + tileX;
    if (index >= m_dirtyTiles.size()) {
        rebuildDirtyTiles();
        return;
    }

    const size_t width = static_cast<size_t>(m_canvasWidth);
    const size_t height = static_cast<size_t>(m_canvasHeight);
    if (width > std::numeric_limits<size_t>::max() / height / 4
        || m_pixels.size() != width * height * 4
        || m_savedSnapshot.pixels.size() != m_pixels.size()) {
        rebuildDirtyTiles();
        return;
    }
    const size_t rowBytes = static_cast<size_t>(tile.width) * 4;
    bool differs = false;
    for (int row = 0; row < tile.height && !differs; ++row) {
        const size_t offset =
            (static_cast<size_t>(tile.y + row) * width + static_cast<size_t>(tile.x)) * 4;
        differs = std::memcmp(m_pixels.data() + offset,
                              m_savedSnapshot.pixels.data() + offset,
                              rowBytes) != 0;
    }

    if (differs && !m_dirtyTiles[index]) {
        m_dirtyTiles[index] = 1;
        ++m_dirtyTileCount;
    } else if (!differs && m_dirtyTiles[index]) {
        m_dirtyTiles[index] = 0;
        if (m_dirtyTileCount > 0) --m_dirtyTileCount;
    }
}

void DrawingApp::updateDirtyFlag() {
    m_dirty = m_canvasWidth != m_savedSnapshot.width
        || m_canvasHeight != m_savedSnapshot.height
        || m_dirtyTileCount > 0;
}

void DrawingApp::beginDirtyTileTracking() {
    const size_t rows = m_canvasHeight > 0
        ? (static_cast<size_t>(m_canvasHeight - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t tileCount = rows != 0
        && m_dirtyTileColumns > std::numeric_limits<size_t>::max() / rows
        ? 0
        : m_dirtyTileColumns * rows;
    if (m_dirtyTileWidth != m_canvasWidth || m_dirtyTileHeight != m_canvasHeight
        || m_dirtyTiles.size() != tileCount) {
        rebuildDirtyTiles();
    }
    m_dirtyTrackingTiles.resize(m_dirtyTiles.size(), 0);
    m_dirtyTrackedTileIndices.clear();
}

void DrawingApp::observeFillSpanWrite(void* context, int y, int left, int right) {
    auto* drawing = static_cast<DrawingApp*>(context);
    if (!drawing) return;

    drawing->noteDirtySpanWrite(y, left, right);
    const int firstTileX = left / kHistoryTileSize;
    const int lastTileX = right / kHistoryTileSize;
    for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
        drawing->captureSparseHistoryTile(tileX * kHistoryTileSize, y);
    }
}

void DrawingApp::noteDirtyTileWrite(int x, int y) {
    if (x < 0 || y < 0 || x >= m_canvasWidth || y >= m_canvasHeight
        || m_dirtyTileColumns == 0) {
        return;
    }
    const size_t index =
        (static_cast<size_t>(y) / kHistoryTileSize) * m_dirtyTileColumns
        + static_cast<size_t>(x) / kHistoryTileSize;
    if (index >= m_dirtyTrackingTiles.size() || m_dirtyTrackingTiles[index]) return;
    m_dirtyTrackingTiles[index] = 1;
    m_dirtyTrackedTileIndices.push_back(index);
}

void DrawingApp::noteDirtySpanWrite(int y, int left, int right) {
    if (y < 0 || y >= m_canvasHeight || left < 0 || right < left
        || right >= m_canvasWidth) {
        return;
    }
    const int firstTileX = left / kHistoryTileSize;
    const int lastTileX = right / kHistoryTileSize;
    for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
        noteDirtyTileWrite(tileX * kHistoryTileSize, y);
    }
}

void DrawingApp::finishDirtyTileTracking() {
    if (m_canvasWidth != m_savedSnapshot.width
        || m_canvasHeight != m_savedSnapshot.height
        || m_savedSnapshot.pixels.size() != m_pixels.size()) {
        rebuildDirtyTiles();
    } else {
        for (const size_t index : m_dirtyTrackedTileIndices) {
            const size_t tileX = index % m_dirtyTileColumns;
            const size_t tileY = index / m_dirtyTileColumns;
            CanvasTileSnapshot tile;
            tile.x = static_cast<int>(tileX * kHistoryTileSize);
            tile.y = static_cast<int>(tileY * kHistoryTileSize);
            tile.width = std::min(kHistoryTileSize, m_canvasWidth - tile.x);
            tile.height = std::min(kHistoryTileSize, m_canvasHeight - tile.y);
            refreshDirtyTile(tile);
        }
        updateDirtyFlag();
    }
    for (const size_t index : m_dirtyTrackedTileIndices) {
        if (index < m_dirtyTrackingTiles.size()) {
            m_dirtyTrackingTiles[index] = 0;
        }
    }
    m_dirtyTrackedTileIndices.clear();
}

void DrawingApp::undoCanvas() {
    if (m_sparseHistoryPending) endActiveStroke();
    if (m_undoStack.empty()) {
        setStatus("Nothing to undo.");
        return;
    }

    CanvasHistoryEntry entry = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    toggleSparseHistoryTiles(entry);
    m_redoStack.push_back(std::move(entry));
    for (const auto& tile : m_redoStack.back().tiles) refreshDirtyTile(tile);
    updateDirtyFlag();
    clearDiscardArm();
    markTextureDirty();
    setStatus("Undo.");
}

void DrawingApp::redoCanvas() {
    if (m_sparseHistoryPending) endActiveStroke();
    if (m_redoStack.empty()) {
        setStatus("Nothing to redo.");
        return;
    }

    CanvasHistoryEntry entry = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    toggleSparseHistoryTiles(entry);
    m_undoStack.push_back(std::move(entry));
    for (const auto& tile : m_undoStack.back().tiles) refreshDirtyTile(tile);
    updateDirtyFlag();
    clearDiscardArm();
    markTextureDirty();
    setStatus("Redo.");
}

void DrawingApp::syncTexture(SDL_Renderer* renderer) {
    if (!renderer || m_canvasWidth <= 0 || m_canvasHeight <= 0) return;

    // Recreate only when missing or canvas size changed — not on every paint stroke.
    if (m_canvasTexture) {
        int texW = 0;
        int texH = 0;
        if (SDL_QueryTexture(m_canvasTexture, nullptr, nullptr, &texW, &texH) != 0
            || texW != m_canvasWidth
            || texH != m_canvasHeight) {
            SDL_DestroyTexture(m_canvasTexture);
            m_canvasTexture = nullptr;
            m_textureDirty = true;
        }
    }

    if (!m_canvasTexture) {
        // ABGR8888 stores bytes as R,G,B,A in memory on little-endian (RGBA8888 swaps R/B).
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        constexpr Uint32 kCanvasPixelFormat = SDL_PIXELFORMAT_RGBA8888;
#else
        constexpr Uint32 kCanvasPixelFormat = SDL_PIXELFORMAT_ABGR8888;
#endif
        m_canvasTexture = SDL_CreateTexture(
            renderer,
            kCanvasPixelFormat,
            SDL_TEXTUREACCESS_STREAMING,
            m_canvasWidth,
            m_canvasHeight
        );

        if (!m_canvasTexture) return;
        m_textureDirty = true;
    }

    // Upload CPU pixels only when the canvas content actually changed.
    if (!m_textureDirty) return;

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_canvasTexture, nullptr, &pixels, &pitch) == 0) {
        const int rowBytes = m_canvasWidth * 4;
        for (int y = 0; y < m_canvasHeight; ++y) {
            std::memcpy(
                static_cast<uint8_t*>(pixels) + static_cast<size_t>(y) * static_cast<size_t>(pitch),
                m_pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(rowBytes),
                static_cast<size_t>(rowBytes)
            );
        }
        SDL_UnlockTexture(m_canvasTexture);
        m_textureDirty = false;
    }
}

int DrawingApp::brushRadius() const {
    switch (m_brush) {
        case BrushSize::Small:  return 2;
        case BrushSize::Medium: return 5;
        case BrushSize::Large:  return 10;
    }
    return 5;
}

uint8_t DrawingApp::activeRed() const {
    if (m_tool == Tool::Eraser) return kCanvasBackgroundR;
    if (m_usingCustomColor) return m_customR;
    return kColors[m_colorIndex].r;
}

uint8_t DrawingApp::activeGreen() const {
    if (m_tool == Tool::Eraser) return kCanvasBackgroundG;
    if (m_usingCustomColor) return m_customG;
    return kColors[m_colorIndex].g;
}

uint8_t DrawingApp::activeBlue() const {
    if (m_tool == Tool::Eraser) return kCanvasBackgroundB;
    if (m_usingCustomColor) return m_customB;
    return kColors[m_colorIndex].b;
}

