DrawingApp::DrawingApp(TTF_Font* font, monolith::fs::Filesystem* fs, const std::string& initialPath)
    : m_font(font), m_fs(fs), m_pendingInitialPath(initialPath)
{
    setStatus("Pen ready. Drag to draw. Ctrl+S save, Ctrl+O open, Ctrl+Z undo.");
}

DrawingApp::~DrawingApp() {
    if (m_canvasTexture) {
        SDL_DestroyTexture(m_canvasTexture);
        m_canvasTexture = nullptr;
    }
}

void DrawingApp::resizeCanvas(int width, int height, bool preserveContent) {
    width = std::clamp(width, 1, monolith::drawing::kMaxCanvasDimension);
    height = std::clamp(height, 1, monolith::drawing::kMaxCanvasDimension);

    if (width == m_canvasWidth && height == m_canvasHeight) return;

    std::vector<uint8_t> newPixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (size_t i = 0; i < newPixels.size(); i += 4) {
        newPixels[i + 0] = kCanvasBackgroundR;
        newPixels[i + 1] = kCanvasBackgroundG;
        newPixels[i + 2] = kCanvasBackgroundB;
        newPixels[i + 3] = 255;
    }

    if (preserveContent && !m_pixels.empty() && m_canvasWidth > 0 && m_canvasHeight > 0) {
        const int copyW = std::min(width, m_canvasWidth);
        const int copyH = std::min(height, m_canvasHeight);
        for (int y = 0; y < copyH; ++y) {
            for (int x = 0; x < copyW; ++x) {
                const size_t src = (static_cast<size_t>(y) * static_cast<size_t>(m_canvasWidth) + static_cast<size_t>(x)) * 4;
                const size_t dst = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
                newPixels[dst + 0] = m_pixels[src + 0];
                newPixels[dst + 1] = m_pixels[src + 1];
                newPixels[dst + 2] = m_pixels[src + 2];
                newPixels[dst + 3] = 255;
            }
        }
    }

    m_pixels = std::move(newPixels);
    m_canvasWidth = width;
    m_canvasHeight = height;
    markTextureDirty();
}

void DrawingApp::clearCanvas(bool recordUndo) {
    if (recordUndo) beginSparseHistory();
    beginDirtyTileTracking();
    bool changed = false;
    for (int tileY = 0; tileY < m_canvasHeight; tileY += kHistoryTileSize) {
        for (int tileX = 0; tileX < m_canvasWidth; tileX += kHistoryTileSize) {
            const int tileWidth = std::min(kHistoryTileSize, m_canvasWidth - tileX);
            const int tileHeight = std::min(kHistoryTileSize, m_canvasHeight - tileY);
            bool tileChanged = false;
            for (int row = 0; row < tileHeight && !tileChanged; ++row) {
                const size_t rowOffset =
                    (static_cast<size_t>(tileY + row) * static_cast<size_t>(m_canvasWidth)
                     + static_cast<size_t>(tileX)) * 4;
                for (int x = 0; x < tileWidth; ++x) {
                    const size_t i = rowOffset + static_cast<size_t>(x) * 4;
                    if (m_pixels[i + 0] != kCanvasBackgroundR
                        || m_pixels[i + 1] != kCanvasBackgroundG
                        || m_pixels[i + 2] != kCanvasBackgroundB
                        || m_pixels[i + 3] != 255) {
                        tileChanged = true;
                        break;
                    }
                }
            }
            if (!tileChanged) continue;

            changed = true;
            noteDirtyTileWrite(tileX, tileY);
            if (recordUndo) captureSparseHistoryTile(tileX, tileY);
            for (int row = 0; row < tileHeight; ++row) {
                const size_t rowOffset =
                    (static_cast<size_t>(tileY + row) * static_cast<size_t>(m_canvasWidth)
                     + static_cast<size_t>(tileX)) * 4;
                for (int x = 0; x < tileWidth; ++x) {
                    const size_t i = rowOffset + static_cast<size_t>(x) * 4;
                    m_pixels[i + 0] = kCanvasBackgroundR;
                    m_pixels[i + 1] = kCanvasBackgroundG;
                    m_pixels[i + 2] = kCanvasBackgroundB;
                    m_pixels[i + 3] = 255;
                }
            }
        }
    }
    if (recordUndo) {
        if (changed) recordSparseHistoryChange();
        finishSparseHistory();
    }
    if (!changed) {
        finishDirtyTileTracking();
        if (recordUndo) setStatus("Canvas already clear.");
        return;
    }

    m_dirty = true;
    finishDirtyTileTracking();
    clearDiscardArm();
    markTextureDirty();
    if (recordUndo) {
        setStatus("Canvas cleared.");
    }
}

void DrawingApp::markTextureDirty() {
    m_textureDirty = true;
}

void DrawingApp::pushUndoHistoryEntry(CanvasHistoryEntry entry) {
    if (entry.width <= 0 || entry.height <= 0) return;
    const size_t width = static_cast<size_t>(entry.width);
    const size_t height = static_cast<size_t>(entry.height);
    if (width > std::numeric_limits<size_t>::max() / height / 4) return;
    if (entry.tiles.empty()) return;

    size_t entryBytes = 0;
    for (const auto& tile : entry.tiles) {
        if (tile.x < 0 || tile.y < 0 || tile.width <= 0 || tile.height <= 0
            || static_cast<size_t>(tile.x) > width
            || static_cast<size_t>(tile.y) > height
            || static_cast<size_t>(tile.width) > width - static_cast<size_t>(tile.x)
            || static_cast<size_t>(tile.height) > height - static_cast<size_t>(tile.y)) {
            return;
        }
        const size_t tileWidth = static_cast<size_t>(tile.width);
        const size_t tileHeight = static_cast<size_t>(tile.height);
        if (tileWidth > std::numeric_limits<size_t>::max() / tileHeight / 4) return;
        const size_t tileBytes = tileWidth * tileHeight * 4;
        if (tile.pixels.size() != tileBytes) return;
        if (tileBytes > kMaxHistoryBytes - entryBytes) {
            entryBytes = kMaxHistoryBytes + 1;
            break;
        }
        entryBytes += tileBytes;
    }

    m_redoStack.clear();
    if (entryBytes > kMaxHistoryBytes) {
        m_undoStack.clear();
        return;
    }

    auto stateBytes = [](const CanvasHistoryEntry& state) {
        size_t bytes = 0;
        for (const auto& tile : state.tiles) bytes += tile.pixels.size();
        return bytes;
    };
    size_t historyBytes = 0;
    for (const auto& state : m_undoStack) {
        historyBytes += stateBytes(state);
    }
    const size_t availableBytes = kMaxHistoryBytes - entryBytes;
    while (!m_undoStack.empty()
           && (m_undoStack.size() >= kMaxHistoryStates || historyBytes > availableBytes)) {
        historyBytes -= stateBytes(m_undoStack.front());
        m_undoStack.erase(m_undoStack.begin());
    }

    m_undoStack.push_back(std::move(entry));
}

void DrawingApp::beginSparseHistory() {
    m_sparseHistoryEntry = {};
    m_sparseHistoryEntry.width = m_canvasWidth;
    m_sparseHistoryEntry.height = m_canvasHeight;
    m_sparseHistoryTileColumns = m_canvasWidth > 0
        ? (static_cast<size_t>(m_canvasWidth - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t tileRows = m_canvasHeight > 0
        ? (static_cast<size_t>(m_canvasHeight - 1) / kHistoryTileSize) + 1
        : 0;
    const size_t tileCount = tileRows != 0
        && m_sparseHistoryTileColumns > std::numeric_limits<size_t>::max() / tileRows
        ? 0
        : m_sparseHistoryTileColumns * tileRows;
    if (m_dirtyTileWidth != m_canvasWidth || m_dirtyTileHeight != m_canvasHeight
        || m_dirtyTiles.size() != tileCount) {
        rebuildDirtyTiles();
    }
    m_sparseHistoryCapturedTiles.resize(tileCount, 0);
    m_sparseHistoryCapturedTileIndices.clear();
    m_sparseHistoryBytes = 0;
    m_sparseHistoryOverflowed = false;
    m_sparseHistoryPending = true;
    m_sparseHistoryChanged = false;
}

void DrawingApp::recordSparseHistoryChange() {
    if (!m_sparseHistoryPending || m_sparseHistoryChanged) return;
    m_sparseHistoryChanged = m_sparseHistoryOverflowed || !m_sparseHistoryEntry.tiles.empty();
}

void DrawingApp::finishSparseHistory() {
    for (const size_t index : m_sparseHistoryCapturedTileIndices) {
        if (index < m_sparseHistoryCapturedTiles.size()) {
            m_sparseHistoryCapturedTiles[index] = 0;
        }
    }
    m_sparseHistoryCapturedTileIndices.clear();
    if (m_sparseHistoryPending && m_sparseHistoryChanged && !m_sparseHistoryOverflowed) {
        pushUndoHistoryEntry(std::move(m_sparseHistoryEntry));
    }
    m_sparseHistoryPending = false;
    m_sparseHistoryChanged = false;
    m_sparseHistoryEntry = {};
    m_sparseHistoryCapturedTiles.clear();
    m_sparseHistoryTileColumns = 0;
    m_sparseHistoryBytes = 0;
    m_sparseHistoryOverflowed = false;
}

void DrawingApp::observeSparseHistoryPixelWrite(void* context, int x, int y) {
    if (context) static_cast<DrawingApp*>(context)->captureSparseHistoryTile(x, y);
}

void DrawingApp::captureSparseHistoryTile(int x, int y) {
    if (x < 0 || y < 0 || x >= m_canvasWidth || y >= m_canvasHeight) {
        return;
    }
    if (m_dirtyTileWidth != m_canvasWidth || m_dirtyTileHeight != m_canvasHeight) {
        rebuildDirtyTiles();
    }

    const size_t dirtyTileX = static_cast<size_t>(x) / kHistoryTileSize;
    const size_t dirtyTileY = static_cast<size_t>(y) / kHistoryTileSize;
    const size_t dirtyTileIndex = dirtyTileY * m_dirtyTileColumns + dirtyTileX;
    if (dirtyTileIndex < m_dirtyTiles.size() && !m_dirtyTiles[dirtyTileIndex]) {
        markDirtyTile(dirtyTileIndex);
    }
    if (!m_sparseHistoryPending || m_sparseHistoryTileColumns == 0
        || m_sparseHistoryOverflowed) {
        return;
    }

    const size_t tileX = static_cast<size_t>(x) / kHistoryTileSize;
    const size_t tileY = static_cast<size_t>(y) / kHistoryTileSize;
    const size_t tileIndex = tileY * m_sparseHistoryTileColumns + tileX;
    if (tileIndex >= m_sparseHistoryCapturedTiles.size()) return;
    if (m_sparseHistoryCapturedTiles[tileIndex]) return;
    m_sparseHistoryCapturedTiles[tileIndex] = 1;
    m_sparseHistoryCapturedTileIndices.push_back(tileIndex);

    CanvasTileSnapshot tile;
    tile.x = static_cast<int>(tileX * kHistoryTileSize);
    tile.y = static_cast<int>(tileY * kHistoryTileSize);
    tile.width = std::min(kHistoryTileSize, m_canvasWidth - tile.x);
    tile.height = std::min(kHistoryTileSize, m_canvasHeight - tile.y);
    const size_t rowBytes = static_cast<size_t>(tile.width) * 4;
    const size_t tileBytes = rowBytes * static_cast<size_t>(tile.height);
    if (tileBytes > kMaxHistoryBytes - m_sparseHistoryBytes) {
        m_sparseHistoryOverflowed = true;
        m_sparseHistoryChanged = true;
        m_sparseHistoryEntry.tiles.clear();
        std::vector<CanvasTileSnapshot>().swap(m_sparseHistoryEntry.tiles);
        m_undoStack.clear();
        m_redoStack.clear();
        return;
    }
    tile.pixels.resize(tileBytes);
    for (int row = 0; row < tile.height; ++row) {
        const size_t source =
            (static_cast<size_t>(tile.y + row) * static_cast<size_t>(m_canvasWidth)
             + static_cast<size_t>(tile.x)) * 4;
        std::memcpy(tile.pixels.data() + static_cast<size_t>(row) * rowBytes,
                    m_pixels.data() + source, rowBytes);
    }
    m_sparseHistoryEntry.tiles.push_back(std::move(tile));
    m_sparseHistoryBytes += tileBytes;
}

void DrawingApp::toggleSparseHistoryTiles(CanvasHistoryEntry& entry) {
    if (entry.width != m_canvasWidth || entry.height != m_canvasHeight) return;
    const size_t canvasStride = static_cast<size_t>(m_canvasWidth) * 4;
    for (auto& tile : entry.tiles) {
        const size_t tileStride = static_cast<size_t>(tile.width) * 4;
        for (int row = 0; row < tile.height; ++row) {
            const size_t canvasOffset =
                static_cast<size_t>(tile.y + row) * canvasStride
                + static_cast<size_t>(tile.x) * 4;
            const size_t tileOffset = static_cast<size_t>(row) * tileStride;
            for (size_t byte = 0; byte < tileStride; ++byte) {
                std::swap(m_pixels[canvasOffset + byte], tile.pixels[tileOffset + byte]);
            }
        }
    }
}

