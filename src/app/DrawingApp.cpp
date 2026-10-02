#include "DrawingApp.hpp"
#include "FilePath.hpp"
#include "DrawingRaster.hpp"
#include "Utf8.hpp"
#include "../detail/RendererClip.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace monolith::app {

namespace {
constexpr int kToolbarPadding = 8;
constexpr int kToolbarButtonHeight = 22;
constexpr int kToolbarGap = 6;
constexpr int kStatusBarHeight = 22;
constexpr int kSwatchSize = 18;
constexpr int kSwatchGap = 4;
constexpr size_t kMaxHistoryStates = 32;
constexpr size_t kMaxHistoryBytes = 64 * 1024 * 1024;
constexpr uint8_t kCanvasBackgroundR = 245;
constexpr uint8_t kCanvasBackgroundG = 245;
constexpr uint8_t kCanvasBackgroundB = 248;

bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

} // namespace

using monolith::detail::RendererClipState;
using monolith::detail::captureRendererClip;
using monolith::detail::restoreRendererClip;
using monolith::detail::intersectRendererClip;

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
    width = std::max(1, width);
    height = std::max(1, height);

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
    m_dirtyTextureRect = {0, 0, m_canvasWidth, m_canvasHeight};
}

void DrawingApp::markTextureRegionDirty(int x, int y, int width, int height) {
    if (m_canvasWidth <= 0 || m_canvasHeight <= 0 || width <= 0 || height <= 0) return;

    const long long left = std::max(0LL, static_cast<long long>(x));
    const long long top = std::max(0LL, static_cast<long long>(y));
    const long long right = std::min(static_cast<long long>(m_canvasWidth),
                                     static_cast<long long>(x) + width);
    const long long bottom = std::min(static_cast<long long>(m_canvasHeight),
                                      static_cast<long long>(y) + height);
    if (left >= right || top >= bottom) return;

    const SDL_Rect region{static_cast<int>(left), static_cast<int>(top),
                          static_cast<int>(right - left),
                          static_cast<int>(bottom - top)};
    if (!m_textureDirty || m_dirtyTextureRect.w <= 0 || m_dirtyTextureRect.h <= 0) {
        m_dirtyTextureRect = region;
        m_textureDirty = true;
        return;
    }

    const int unionLeft = std::min(m_dirtyTextureRect.x, region.x);
    const int unionTop = std::min(m_dirtyTextureRect.y, region.y);
    const int unionRight = std::max(m_dirtyTextureRect.x + m_dirtyTextureRect.w,
                                    region.x + region.w);
    const int unionBottom = std::max(m_dirtyTextureRect.y + m_dirtyTextureRect.h,
                                     region.y + region.h);
    m_dirtyTextureRect = {unionLeft, unionTop, unionRight - unionLeft,
                          unionBottom - unionTop};
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
    if (!context) return;
    auto* drawing = static_cast<DrawingApp*>(context);
    drawing->markTextureRegionDirty(x, y, 1, 1);
    drawing->captureSparseHistoryTile(x, y);
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
        markTextureRegionDirty(tile.x, tile.y, tile.width, tile.height);
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

    drawing->markTextureRegionDirty(left, y, right - left + 1, 1);
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
            markTextureDirty();
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
        markTextureDirty();
    }

    // Upload CPU pixels only when the canvas content actually changed.
    if (!m_textureDirty) return;

    SDL_Rect uploadRect = m_dirtyTextureRect;
    if (uploadRect.w <= 0 || uploadRect.h <= 0) {
        uploadRect = {0, 0, m_canvasWidth, m_canvasHeight};
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_canvasTexture, &uploadRect, &pixels, &pitch) == 0) {
        const int rowBytes = m_canvasWidth * 4;
        const size_t uploadRowBytes = static_cast<size_t>(uploadRect.w) * 4;
        for (int row = 0; row < uploadRect.h; ++row) {
            std::memcpy(
                static_cast<uint8_t*>(pixels) + static_cast<size_t>(row) * static_cast<size_t>(pitch),
                m_pixels.data()
                    + static_cast<size_t>(uploadRect.y + row) * static_cast<size_t>(rowBytes)
                    + static_cast<size_t>(uploadRect.x) * 4,
                uploadRowBytes
            );
        }
        SDL_UnlockTexture(m_canvasTexture);
        m_textureDirty = false;
        m_dirtyTextureRect = {};
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

bool DrawingApp::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    return monolith::drawing::setPixel(
        m_pixels, m_canvasWidth, m_canvasHeight, x, y, r, g, b,
        &DrawingApp::observeSparseHistoryPixelWrite, this);
}

bool DrawingApp::commitShape(int x0, int y0, int x1, int y1) {
    const uint8_t r = activeRed();
    const uint8_t g = activeGreen();
    const uint8_t b = activeBlue();
    bool changed = false;
    if (m_tool == Tool::Line) {
        changed = monolith::drawing::drawLine(
            m_pixels, m_canvasWidth, m_canvasHeight, x0, y0, x1, y1, r, g, b,
            &DrawingApp::observeSparseHistoryPixelWrite, this);
    } else if (m_tool == Tool::Rect) {
        changed = monolith::drawing::drawRect(
            m_pixels, m_canvasWidth, m_canvasHeight, x0, y0, x1, y1, r, g, b,
            &DrawingApp::observeSparseHistoryPixelWrite, this);
    }
    return changed;
}

bool DrawingApp::stampBrush(int x, int y) {
    return monolith::drawing::stampBrush(
        m_pixels, m_canvasWidth, m_canvasHeight, x, y, brushRadius(),
        activeRed(), activeGreen(), activeBlue(),
        &DrawingApp::observeSparseHistoryPixelWrite, this);
}

void DrawingApp::floodFill(int x, int y) {
    uint8_t targetR = 0;
    uint8_t targetG = 0;
    uint8_t targetB = 0;
    if (!monolith::drawing::getPixel(
            m_pixels, m_canvasWidth, m_canvasHeight, x, y,
            targetR, targetG, targetB)) {
        return;
    }

    const uint8_t fillR = activeRed();
    const uint8_t fillG = activeGreen();
    const uint8_t fillB = activeBlue();

    if (targetR == fillR && targetG == fillG && targetB == fillB) {
        setStatus("Fill: color already matches.");
        return;
    }

    beginSparseHistory();
    beginDirtyTileTracking();
    const size_t filledPixels = monolith::drawing::fillRegion(
        m_pixels, m_canvasWidth, m_canvasHeight, x, y, fillR, fillG, fillB,
        &DrawingApp::observeFillSpanWrite, this);
    if (filledPixels > 0) recordSparseHistoryChange();
    finishSparseHistory();

    m_dirty = true;
    finishDirtyTileTracking();
    clearDiscardArm();
    setStatus("Filled region.");
}

void DrawingApp::pickColorAt(int x, int y) {
    if (!monolith::drawing::getPixel(
            m_pixels, m_canvasWidth, m_canvasHeight, x, y,
            m_customR, m_customG, m_customB)) {
        return;
    }
    m_usingCustomColor = true;
    m_tool = Tool::Pen;

    std::ostringstream oss;
    oss << "Picked RGB " << static_cast<int>(m_customR) << ","
        << static_cast<int>(m_customG) << "," << static_cast<int>(m_customB)
        << ". Tool: Pen";
    setStatus(oss.str());
}

bool DrawingApp::drawStroke(int x0, int y0, int x1, int y1) {
    return monolith::drawing::drawBrushStroke(
        m_pixels, m_canvasWidth, m_canvasHeight, x0, y0, x1, y1, brushRadius(),
        activeRed(), activeGreen(), activeBlue(),
        &DrawingApp::observeSparseHistoryPixelWrite, this);
}

bool DrawingApp::isInCanvas(int x, int y) const {
    const int displayHeight = m_clientHeight - m_canvasTop - m_statusBarHeight;
    return displayHeight > 0
        && x >= 0 && y >= m_canvasTop
        && x < m_clientWidth
        && y < m_canvasTop + displayHeight;
}

int DrawingApp::getToolbarButtonHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 0;
    return std::max(kToolbarButtonHeight, fontHeight + 4);
}

int DrawingApp::getToolbarHeight() const {
    const int buttonHeight = getToolbarButtonHeight();
    return kToolbarPadding
        + buttonHeight * 3
        + kToolbarGap * 2
        + 10;
}

int DrawingApp::getStatusBarHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 0;
    return std::max(kStatusBarHeight, fontHeight + 8);
}

void DrawingApp::updateLayoutMetrics() {
    m_canvasTop = getToolbarHeight();
    m_statusBarHeight = getStatusBarHeight();
}

void DrawingApp::canvasPointFromClient(int clientX, int clientY, int& outX, int& outY) const {
    const int displayWidth = std::max(1, m_clientWidth);
    const int displayHeight = std::max(1, m_clientHeight - m_canvasTop - m_statusBarHeight);
    const int displayY = std::clamp(clientY - m_canvasTop, 0, displayHeight - 1);
    outX = std::clamp(
        static_cast<int>((static_cast<long long>(clientX) * m_canvasWidth) / displayWidth),
        0,
        std::max(0, m_canvasWidth - 1));
    outY = std::clamp(
        static_cast<int>((static_cast<long long>(displayY) * m_canvasHeight) / displayHeight),
        0,
        std::max(0, m_canvasHeight - 1));
}

std::string DrawingApp::defaultSavePath() {
    if (!m_fs) return "/home/monolith/drawings/sketch.modr";

    const std::string baseDir = "/home/monolith/drawings";
    m_fs->createDirectory("/home/monolith");
    m_fs->createDirectory(baseDir);

    for (std::uint64_t i = 1;; ++i) {
        std::ostringstream oss;
        oss << baseDir << "/sketch";
        if (i > 1) oss << "_" << i;
        oss << ".modr";
        const std::string candidate = oss.str();
        if (!m_fs->exists(candidate)) {
            return candidate;
        }
    }
    return baseDir + "/sketch.modr";
}

bool DrawingApp::saveToPath(const std::string& virtualPath) {
    if (!m_fs) {
        clearDiscardArm();
        setStatus("Save failed: filesystem not available.");
        return false;
    }
    if (m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        clearDiscardArm();
        setStatus("Save failed: canvas is empty.");
        return false;
    }

    std::string path = m_fs->normalize(virtualPath);
    if (!hasCaseInsensitiveSuffix(path, ".modr")) {
        path += ".modr";
    }

    const bool wasExisting = m_fs->exists(path);

    std::string parent = path;
    const size_t slash = parent.find_last_of('/');
    if (slash != std::string::npos) {
        parent = parent.substr(0, slash);
        if (!parent.empty()) {
            m_fs->createDirectory(parent);
        }
    }

    const std::string blob = monolith::drawing::encodeModr(m_canvasWidth, m_canvasHeight, m_pixels);
    if (blob.empty()) {
        clearDiscardArm();
        setStatus("Save failed: could not encode canvas.");
        return false;
    }

    if (!m_fs->writeFile(path, blob)) {
        clearDiscardArm();
        setStatus("Save failed: could not write file.");
        return false;
    }

    m_filePath = path;
    m_savedSnapshot = {m_canvasWidth, m_canvasHeight, m_pixels};
    m_dirty = false;
    clearDirtyTiles();
    clearDiscardArm();

    if (auto* ctrl = getController()) {
        // Claim the singleton before broadcasting so a synchronous observer
        // opening this path focuses this Drawing instead of creating a duplicate.
        ctrl->bindDrawingFile(path);
        if (!wasExisting) {
            ctrl->notifyVirtualPathCreated(path);
        } else {
            // The shell broadcasts synchronously, including back to this Drawing
            // window. Do not report the window's own save as an external change.
            m_suppressChangedNotification = true;
            ctrl->notifyVirtualPathChanged(path);
            m_suppressChangedNotification = false;
        }
    }

    size_t nameStart = path.find_last_of('/');
    const std::string baseName = (nameStart != std::string::npos) ? path.substr(nameStart + 1) : path;
    if (auto* ctrl = getController()) {
        ctrl->setTitle("Drawing - " + baseName);
    }

    setStatus("Saved: " + path);
    return true;
}

bool DrawingApp::loadFromPath(const std::string& virtualPath) {
    if (!m_fs) {
        setStatus("Open failed: filesystem not available.");
        return false;
    }

    const std::string path = m_fs->normalize(virtualPath);
    if (!hasCaseInsensitiveSuffix(path, ".modr")) {
        setStatus("Open failed: Drawing files must use .modr.");
        return false;
    }

    std::string blob;
    if (!m_fs->readFile(path, blob)) {
        setStatus("Open failed: could not read file.");
        return false;
    }
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    if (!monolith::drawing::decodeModr(blob, width, height, rgba)) {
        setStatus("Open failed: not a valid .modr drawing file.");
        return false;
    }

    resizeCanvas(width, height, false);
    m_pixels = std::move(rgba);

    m_filePath = path;
    m_savedSnapshot = {m_canvasWidth, m_canvasHeight, m_pixels};
    m_dirty = false;
    clearDirtyTiles();
    m_undoStack.clear();
    m_redoStack.clear();
    markTextureDirty();

    size_t nameStart = path.find_last_of('/');
    const std::string baseName = (nameStart != std::string::npos) ? path.substr(nameStart + 1) : path;
    if (auto* ctrl = getController()) {
        ctrl->setTitle("Drawing - " + baseName);
        ctrl->bindDrawingFile(path);
    }

    setStatus("Opened: " + path);
    return true;
}

void DrawingApp::setStatus(const std::string& message) {
    if (m_statusMessage == message) return;
    m_statusMessage = message;
}

void DrawingApp::onBoundFileMoved(const std::string& oldPath,
                                  const std::string& newPath) {
    if (newPath.empty()) return;
    const std::string normalizedNewPath = m_fs ? m_fs->normalize(newPath) : newPath;

    remapPathPrompt(oldPath, newPath);

    m_filePath = normalizedNewPath;
    clearDiscardArm();

    const size_t nameStart = normalizedNewPath.find_last_of('/');
    const std::string baseName = nameStart != std::string::npos
        ? normalizedNewPath.substr(nameStart + 1)
        : normalizedNewPath;
    if (auto* ctrl = getController()) {
        ctrl->setTitle("Drawing - " + baseName);
    }
    setStatus("File moved: " + normalizedNewPath);
}

void DrawingApp::onVirtualPathMoved(const std::string& oldPath,
                                    const std::string& newPath) {
    remapPathPrompt(oldPath, newPath);
}

void DrawingApp::remapPathPrompt(const std::string& oldPath,
                                 const std::string& newPath) {
    if (!m_fs || (m_pathPromptMode != PathPromptMode::Open
                  && m_pathPromptMode != PathPromptMode::Save)) {
        return;
    }

    const std::string oldNormalized = m_fs->normalize(oldPath);
    const std::string normalizedNewPath = m_fs->normalize(newPath);
    if (oldNormalized.empty() || normalizedNewPath.empty() || oldNormalized == "/") return;

    const bool trailingSlash = !m_pathPromptBuffer.empty()
        && m_pathPromptBuffer.back() == '/';
    const std::string oldPrompt = m_pathPromptBuffer;
    const std::size_t oldCursor = m_pathPromptCursorPos;
    const std::string promptPath = m_fs->normalize(m_pathPromptBuffer);
    if (!m_fs->isSameOrDescendant(oldNormalized, promptPath)) return;

    m_pathPromptBuffer = normalizedNewPath + promptPath.substr(oldNormalized.size());
    if (trailingSlash && m_pathPromptBuffer.back() != '/') {
        m_pathPromptBuffer.push_back('/');
    }
    m_pathPromptCursorPos = remapUtf8CursorAfterPrefix(
        oldPrompt, oldCursor, oldNormalized, normalizedNewPath,
        m_pathPromptBuffer);
    m_pathPromptScrollPx = 0;
}

void DrawingApp::onVirtualPathChanged(const std::string& changedPath) {
    if (m_suppressChangedNotification || !m_fs || m_filePath.empty()) return;

    if (m_fs->normalize(changedPath) != m_fs->normalize(m_filePath)) return;

    setStatus("File changed externally; canvas unchanged. Save to overwrite it.");
}

void DrawingApp::onBoundFileRemoved(const std::string& removedPath) {
    if (m_fs && (m_pathPromptMode == PathPromptMode::Open
                 || m_pathPromptMode == PathPromptMode::Save)) {
        const std::string removed = m_fs->normalize(removedPath);
        const std::string promptPath = m_fs->normalize(m_pathPromptBuffer);
        if (m_fs->isSameOrDescendant(removed, promptPath)) {
            const size_t slash = removed.find_last_of('/');
            const std::string parent = slash == 0 ? "/" : removed.substr(0, slash);
            m_pathPromptBuffer = parent == "/" ? "/" : parent + "/";
            m_pathPromptCursorPos = m_pathPromptBuffer.size();
            m_pathPromptScrollPx = 0;
        }
    }

    m_filePath.clear();
    clearDiscardArm();
    if (auto* ctrl = getController()) {
        ctrl->clearDrawingFileBinding();
        ctrl->restoreTrackedInstanceTitle();
    }
    setStatus("File removed: use Save to choose a new .modr path");
}

void DrawingApp::clearDiscardArm() {
    m_discardKind = DiscardKind::None;
    m_discardPath.clear();
}

bool DrawingApp::requestDiscard(DiscardKind kind, const char* statusMessage) {
    if (!m_dirty) {
        clearDiscardArm();
        return true;
    }
    if (m_discardKind == kind) {
        clearDiscardArm();
        return true;
    }
    m_discardKind = kind;
    setStatus(statusMessage);
    return false;
}

bool DrawingApp::allowClose() {
    return requestDiscard(
        DiscardKind::Close,
        "Unsaved changes — close again to discard, or Ctrl+S to save"
    );
}

void DrawingApp::startNewSketch() {
    if (!requestDiscard(
            DiscardKind::New,
            "Unsaved changes — New again to discard, or save first")) {
        return;
    }
    clearCanvas(false);
    m_filePath.clear();
    m_savedSnapshot = {m_canvasWidth, m_canvasHeight, m_pixels};
    m_undoStack.clear();
    m_redoStack.clear();
    m_dirty = false; // blank new sketch is clean
    clearDirtyTiles();
    clearDiscardArm();
    if (auto* ctrl = getController()) {
        ctrl->clearDrawingFileBinding();
        ctrl->restoreTrackedInstanceTitle();
    }
    setStatus("New sketch.");
}

void DrawingApp::beginPathPrompt(PathPromptMode mode) {
    // A new prompt is a new user action. Do not carry a discarded confirmation
    // from an earlier Open/New attempt into it.
    clearDiscardArm();
    // The prompt owns subsequent input, including the matching mouse release.
    // Close any active stroke before that release is intentionally ignored.
    endActiveStroke();
    m_pathPromptMode = mode;
    if (mode == PathPromptMode::Save) {
        m_pathPromptBuffer = m_filePath.empty() ? defaultSavePath() : m_filePath;
        setStatus("Save as (Tab complete, Enter confirm, Esc cancel):");
    } else if (mode == PathPromptMode::Open) {
        m_pathPromptBuffer = "/home/monolith/drawings/";
        setStatus("Open path (Tab complete, Enter confirm, Esc cancel):");
    } else if (mode == PathPromptMode::Rgb) {
        std::ostringstream oss;
        oss << static_cast<int>(activeRed()) << ","
            << static_cast<int>(activeGreen()) << ","
            << static_cast<int>(activeBlue());
        m_pathPromptBuffer = oss.str();
        setStatus("Custom RGB 0-255 as r,g,b (Enter confirm, Esc cancel):");
    }
    m_pathPromptCursorPos = m_pathPromptBuffer.size();
    m_pathPromptScrollPx = 0;
}

void DrawingApp::finishPathPrompt(bool commit) {
    const PathPromptMode mode = m_pathPromptMode;
    const std::string buffer = m_pathPromptBuffer;
    m_pathPromptMode = PathPromptMode::None;
    m_pathPromptBuffer.clear();
    m_pathPromptCursorPos = 0;
    m_pathPromptScrollPx = 0;

    if (!commit) {
        clearDiscardArm();
        setStatus("Cancelled.");
        return;
    }

    if (buffer.empty()) {
        setStatus(mode == PathPromptMode::Rgb ? "RGB cannot be empty." : "Path cannot be empty.");
        return;
    }

    if (mode == PathPromptMode::Rgb) {
        uint8_t r = 0, g = 0, b = 0;
        if (!monolith::drawing::parseRgb(buffer, r, g, b)) {
            setStatus("RGB failed: use r,g,b with each channel 0-255.");
            return;
        }
        m_customR = r;
        m_customG = g;
        m_customB = b;
        m_usingCustomColor = true;
        if (m_tool == Tool::Eraser || m_tool == Tool::Fill || m_tool == Tool::Eyedropper) {
            m_tool = Tool::Pen;
        }
        std::ostringstream oss;
        oss << "Color: custom RGB " << static_cast<int>(r) << ","
            << static_cast<int>(g) << "," << static_cast<int>(b);
        setStatus(oss.str());
        return;
    }

    std::string path = m_fs ? m_fs->normalize(buffer) : buffer;
    if (mode == PathPromptMode::Save && !hasCaseInsensitiveSuffix(path, ".modr")) {
        path += ".modr";
    }

    if (mode == PathPromptMode::Save) {
        if (path != m_filePath) {
            if (auto* ctrl = getController(); ctrl && ctrl->focusDrawingForFile(path)) {
                setStatus("Save failed: file already open");
                return;
            }
        }
        saveToPath(buffer);
    } else if (mode == PathPromptMode::Open) {
        if (auto* ctrl = getController(); ctrl && ctrl->focusDrawingForFile(path)) {
            setStatus("Already open: " + path);
            return;
        }
        if (m_discardKind == DiscardKind::Open && m_discardPath != path) {
            clearDiscardArm();
        }
        if (!requestDiscard(
                DiscardKind::Open,
                "Unsaved changes — open again to discard, or save first")) {
            m_discardPath = path;
            m_pathPromptMode = PathPromptMode::Open;
            m_pathPromptBuffer = path;
            m_pathPromptCursorPos = m_pathPromptBuffer.size();
            m_pathPromptScrollPx = 0;
            return;
        }
        if (loadFromPath(path)) {
            clearDiscardArm();
        }
    }
}

void DrawingApp::completePathPrompt() {
    if (m_pathPromptMode == PathPromptMode::Rgb) return;
    if (!m_fs || m_pathPromptBuffer.empty()) return;

    m_pathPromptCursorPos = std::min(m_pathPromptCursorPos, m_pathPromptBuffer.size());
    if (m_pathPromptBuffer.find('/', m_pathPromptCursorPos) != std::string::npos) return;

    const std::string prefixBuffer = m_pathPromptBuffer.substr(0, m_pathPromptCursorPos);
    const size_t slash = prefixBuffer.find_last_of('/');
    const size_t nameStart = (slash == std::string::npos) ? 0 : slash + 1;
    const std::string dirPart = (slash == std::string::npos) ? "" : prefixBuffer.substr(0, slash);
    const std::string namePrefix = prefixBuffer.substr(nameStart);
    const std::string searchDir = m_fs->normalize(dirPart.empty() ? "/" : dirPart);
    const std::string completionBase = (slash == std::string::npos)
        ? ""
        : ((slash == 0) ? "/" : dirPart + "/");

    std::vector<std::string> matches;
    for (const auto& entry : m_fs->listEntries(searchDir)) {
        if (entry.name.size() < namePrefix.size()) continue;
        if (entry.name.compare(0, namePrefix.size(), namePrefix) != 0) continue;

        std::string completion = completionBase + entry.name;
        const std::string candidatePath = m_fs->normalize(
            searchDir == "/" ? "/" + entry.name : searchDir + "/" + entry.name
        );

        if (m_pathPromptMode == PathPromptMode::Open && !entry.isDirectory
            && !hasCaseInsensitiveSuffix(candidatePath, ".modr")) {
            continue;
        }

        if (entry.isDirectory && !completion.empty() && completion.back() != '/') {
            completion += "/";
        }
        matches.push_back(std::move(completion));
    }

    if (matches.empty()) {
        setStatus("No path matches.");
        return;
    }

    if (matches.size() == 1) {
        const std::string replacement = matches.front().substr(nameStart);
        m_pathPromptBuffer.replace(
            nameStart, m_pathPromptCursorPos - nameStart, replacement);
        m_pathPromptCursorPos = nameStart + replacement.size();
        setStatus("Path completed. Enter confirm, Esc cancel:");
        return;
    }

    const std::string common = utf8CommonPrefix(matches);
    if (common.size() > prefixBuffer.size()) {
        const std::string replacement = common.substr(nameStart);
        m_pathPromptBuffer.replace(
            nameStart, m_pathPromptCursorPos - nameStart, replacement);
        m_pathPromptCursorPos = nameStart + replacement.size();
        setStatus("Path completed to common prefix.");
    } else {
        std::ostringstream oss;
        oss << matches.size() << " matches";
        const size_t previewCount = std::min<size_t>(matches.size(), 3);
        for (size_t i = 0; i < previewCount; ++i) {
            oss << (i == 0 ? ": " : ", ") << matches[i].substr(completionBase.size());
        }
        if (matches.size() > previewCount) {
            oss << ", ...";
        }
        setStatus(oss.str());
    }
}

void DrawingApp::handlePathPromptKey(const SDL_Keysym& keysym) {
    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            finishPathPrompt(true);
            break;
        case SDLK_ESCAPE:
            finishPathPrompt(false);
            break;
        case SDLK_BACKSPACE:
            erasePreviousUtf8Codepoint(m_pathPromptBuffer, m_pathPromptCursorPos);
            break;
        case SDLK_DELETE: {
            m_pathPromptCursorPos = std::min(m_pathPromptCursorPos, m_pathPromptBuffer.size());
            const std::size_t next = utf8NextCodepointStart(
                m_pathPromptBuffer, m_pathPromptCursorPos);
            if (next > m_pathPromptCursorPos) {
                m_pathPromptBuffer.erase(m_pathPromptCursorPos, next - m_pathPromptCursorPos);
            }
            break;
        }
        case SDLK_LEFT:
            m_pathPromptCursorPos = utf8PrevCodepointStart(
                m_pathPromptBuffer, m_pathPromptCursorPos);
            break;
        case SDLK_RIGHT:
            m_pathPromptCursorPos = utf8NextCodepointStart(
                m_pathPromptBuffer, m_pathPromptCursorPos);
            break;
        case SDLK_HOME:
            m_pathPromptCursorPos = 0;
            break;
        case SDLK_END:
            m_pathPromptCursorPos = m_pathPromptBuffer.size();
            break;
        case SDLK_TAB:
            completePathPrompt();
            break;
        default:
            break;
    }
}

void DrawingApp::handlePathPromptText(const char* text) {
    if (!text) return;
    m_pathPromptCursorPos = std::min(m_pathPromptCursorPos, m_pathPromptBuffer.size());
    std::string inserted;
    for (const char* p = text; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 32 && c != 127) {
            inserted.push_back(static_cast<char>(c));
        }
    }
    if (!inserted.empty()) {
        m_pathPromptBuffer.insert(m_pathPromptCursorPos, inserted);
        m_pathPromptCursorPos += inserted.size();
    }
}

void DrawingApp::handleToolbarClick(int x, int y) {
    ensureHitTargets();

    if (pointInRect(x, y, m_btnNew)) {
        startNewSketch();
        return;
    }
    if (pointInRect(x, y, m_btnSave)) {
        if (m_filePath.empty()) {
            beginPathPrompt(PathPromptMode::Save);
        } else {
            saveToPath(m_filePath);
        }
        return;
    }
    if (pointInRect(x, y, m_btnOpen)) {
        beginPathPrompt(PathPromptMode::Open);
        return;
    }
    if (pointInRect(x, y, m_btnUndo)) {
        undoCanvas();
        return;
    }
    if (pointInRect(x, y, m_btnRedo)) {
        redoCanvas();
        return;
    }
    if (pointInRect(x, y, m_btnPen)) {
        m_tool = Tool::Pen;
        setStatus("Tool: Pen");
        return;
    }
    if (pointInRect(x, y, m_btnEraser)) {
        m_tool = Tool::Eraser;
        setStatus("Tool: Eraser");
        return;
    }
    if (pointInRect(x, y, m_btnFill)) {
        m_tool = Tool::Fill;
        setStatus("Tool: Fill (click a region)");
        return;
    }
    if (pointInRect(x, y, m_btnEyedropper)) {
        m_tool = Tool::Eyedropper;
        setStatus("Tool: Pick (click a canvas pixel)");
        return;
    }
    if (pointInRect(x, y, m_btnLine)) {
        m_tool = Tool::Line;
        setStatus("Tool: Line (drag to draw a straight stroke)");
        return;
    }
    if (pointInRect(x, y, m_btnRect)) {
        m_tool = Tool::Rect;
        setStatus("Tool: Rect (drag to draw a rectangle)");
        return;
    }
    if (pointInRect(x, y, m_btnRgb)) {
        beginPathPrompt(PathPromptMode::Rgb);
        return;
    }
    if (pointInRect(x, y, m_btnClear)) {
        clearCanvas();
        return;
    }
    if (pointInRect(x, y, m_btnBrushSmall)) {
        m_brush = BrushSize::Small;
        setStatus("Brush: Small");
        return;
    }
    if (pointInRect(x, y, m_btnBrushMedium)) {
        m_brush = BrushSize::Medium;
        setStatus("Brush: Medium");
        return;
    }
    if (pointInRect(x, y, m_btnBrushLarge)) {
        m_brush = BrushSize::Large;
        setStatus("Brush: Large");
        return;
    }

    for (int i = 0; i < kColorCount; ++i) {
        if (pointInRect(x, y, m_colorSwatches[i])) {
            m_colorIndex = i;
            m_usingCustomColor = false;
            m_tool = Tool::Pen;
            setStatus(std::string("Color: ") + kColors[i].name);
            return;
        }
    }
}

void DrawingApp::drawToolbar(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    ensureHitTargets();

    SDL_Rect toolbar = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        m_canvasTop
    };
    SDL_SetRenderDrawColor(renderer, 32, 34, 40, 255);
    SDL_RenderFillRect(renderer, &toolbar);

    auto drawButton = [&](const SDL_Rect& hitRect, const char* label, bool active) {
        SDL_Rect drawRect = {
            contentRect.x + hitRect.x,
            contentRect.y + hitRect.y,
            hitRect.w,
            hitRect.h
        };

        if (active) {
            SDL_SetRenderDrawColor(renderer, 78, 110, 165, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 48, 52, 60, 255);
        }
        SDL_RenderFillRect(renderer, &drawRect);
        SDL_SetRenderDrawColor(renderer, 78, 84, 96, 255);
        SDL_RenderDrawRect(renderer, &drawRect);

        if (m_font) {
            SDL_Color col = {225, 228, 235, 255};
            const auto texture = m_textTextureCache.get(
                renderer, m_font, label, col);
            if (texture) {
                SDL_Rect dst = {
                    drawRect.x + (drawRect.w - texture.width) / 2,
                    drawRect.y + (drawRect.h - texture.height) / 2,
                    texture.width,
                    texture.height
                };
                SDL_RenderCopy(renderer, texture.handle, nullptr, &dst);
            }
        }
    };

    drawButton(m_btnNew, "New", false);
    drawButton(m_btnSave, "Save", false);
    drawButton(m_btnOpen, "Open", false);
    drawButton(m_btnUndo, "Undo", false);
    drawButton(m_btnRedo, "Redo", false);

    drawButton(m_btnPen, "Pen", m_tool == Tool::Pen);
    drawButton(m_btnEraser, "Eraser", m_tool == Tool::Eraser);
    drawButton(m_btnFill, "Fill", m_tool == Tool::Fill);
    drawButton(m_btnEyedropper, "Pick", m_tool == Tool::Eyedropper);
    drawButton(m_btnLine, "Line", m_tool == Tool::Line);
    drawButton(m_btnRect, "Rect", m_tool == Tool::Rect);
    drawButton(m_btnClear, "Clear", false);
    drawButton(m_btnBrushSmall, "S", m_brush == BrushSize::Small);
    drawButton(m_btnBrushMedium, "M", m_brush == BrushSize::Medium);
    drawButton(m_btnBrushLarge, "L", m_brush == BrushSize::Large);

    drawButton(m_btnRgb, "RGB", m_usingCustomColor);

    for (int i = 0; i < kColorCount; ++i) {
        SDL_Rect swatch = {
            contentRect.x + m_colorSwatches[i].x,
            contentRect.y + m_colorSwatches[i].y,
            kSwatchSize,
            kSwatchSize
        };
        SDL_SetRenderDrawColor(renderer, kColors[i].r, kColors[i].g, kColors[i].b, 255);
        SDL_RenderFillRect(renderer, &swatch);

        if (!m_usingCustomColor && i == m_colorIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &swatch);
            SDL_Rect inner = {swatch.x + 1, swatch.y + 1, swatch.w - 2, swatch.h - 2};
            SDL_RenderDrawRect(renderer, &inner);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 64, 72, 255);
            SDL_RenderDrawRect(renderer, &swatch);
        }
    }

    if (m_usingCustomColor) {
        SDL_Rect custom = {
            contentRect.x + m_btnRgb.x + 2,
            contentRect.y + m_btnRgb.y + m_btnRgb.h + 2,
            14,
            4
        };
        SDL_SetRenderDrawColor(renderer, m_customR, m_customG, m_customB, 255);
        SDL_RenderFillRect(renderer, &custom);
    }
}

void DrawingApp::drawStatusBar(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    const RendererClipState previousClip = captureRendererClip(renderer);
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y + contentRect.h - m_statusBarHeight,
        contentRect.w,
        m_statusBarHeight
    };
    SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255);
    SDL_RenderFillRect(renderer, &bar);

    if (!m_font) return;

    std::string text = m_statusMessage;
    bool promptActive = false;
    int promptCursorPx = 0;
    if (m_pathPromptMode != PathPromptMode::None) {
        m_pathPromptCursorPos = std::min(m_pathPromptCursorPos, m_pathPromptBuffer.size());
        const std::string beforeCursor = m_pathPromptBuffer.substr(0, m_pathPromptCursorPos);
        text += " " + beforeCursor + "_" + m_pathPromptBuffer.substr(m_pathPromptCursorPos);
        const std::string cursorText = m_statusMessage + " " + beforeCursor + "_";
        promptCursorPx = measurePromptCursorWidth(cursorText);
        promptActive = true;
    } else if (m_dirty) {
        text += "  [modified]";
        m_pathPromptScrollPx = 0;
    }

    SDL_Color col = {170, 175, 185, 255};
    const auto texture = m_textTextureCache.get(
        renderer, m_font, text.c_str(), col);
    if (texture) {
        const int visibleWidth = std::max(1, bar.w - 16);
        if (promptActive) {
            if (promptCursorPx - m_pathPromptScrollPx > visibleWidth) {
                m_pathPromptScrollPx = promptCursorPx - visibleWidth;
            } else if (promptCursorPx - m_pathPromptScrollPx < 0) {
                m_pathPromptScrollPx = promptCursorPx;
            }
            const int maxScroll = std::max(0, texture.width - visibleWidth);
            m_pathPromptScrollPx = std::clamp(m_pathPromptScrollPx, 0, maxScroll);
        }
        SDL_Rect clip = {
            contentRect.x + 8,
            bar.y,
            visibleWidth,
            bar.h
        };
        const SDL_Rect effectiveClip = intersectRendererClip(clip, previousClip);
        SDL_RenderSetClipRect(renderer, &effectiveClip);
        SDL_Rect dst = {
            contentRect.x + 8 - (promptActive ? m_pathPromptScrollPx : 0),
            bar.y + (bar.h - texture.height) / 2,
            texture.width,
            texture.height
        };
        SDL_RenderCopy(renderer, texture.handle, nullptr, &dst);
        restoreRendererClip(renderer, previousClip);
    }
}

int DrawingApp::measurePromptCursorWidth(const std::string& text) {
    if (m_promptCursorMeasureValid && m_promptCursorMeasureText == text) {
        return m_promptCursorPixelWidth;
    }
    if (!m_font) return 0;

    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(m_font, text.c_str(), &width, &height) != 0) return 0;

    m_promptCursorMeasureText = text;
    m_promptCursorPixelWidth = width;
    m_promptCursorMeasureValid = true;
    return width;
}

void DrawingApp::onFocusLost() {
    // Start-menu and window-focus transitions do not guarantee a matching
    // mouse release, so never carry a canvas gesture across the boundary.
    endActiveStroke();
}

void DrawingApp::onResize(int clientWidth, int clientHeight) {
    endActiveStroke();
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    updateLayoutMetrics();
    invalidateHitTargets();

    const int canvasW = std::max(1, clientWidth);
    const int canvasH = std::max(1, clientHeight - m_canvasTop - m_statusBarHeight);
    const bool canvasSizeChanged = canvasW != m_canvasWidth || canvasH != m_canvasHeight;
    resizeCanvas(canvasW, canvasH, true);
    if (canvasSizeChanged && m_pendingInitialPath.empty()) {
        m_undoStack.clear();
        m_redoStack.clear();
        if (!m_filePath.empty()) {
            m_dirty = true;
            clearDiscardArm();
        } else if (!m_dirty) {
            m_savedSnapshot = {m_canvasWidth, m_canvasHeight, m_pixels};
        }
        rebuildDirtyTiles();
    }

    if (!m_pendingInitialPath.empty()) {
        const std::string path = m_pendingInitialPath;
        m_pendingInitialPath.clear();
        if (!loadFromPath(path)) {
            // A failed initial open becomes an untitled blank sketch. Keep that
            // fallback canvas clean so its first undo returns to a clean state.
            m_savedSnapshot = {m_canvasWidth, m_canvasHeight, m_pixels};
            m_dirty = false;
            clearDirtyTiles();
        }
    }
}

void DrawingApp::onUiScaleChanged() {
    m_textTextureCache.clear();
    m_promptCursorMeasureValid = false;
    updateLayoutMetrics();
    invalidateHitTargets();
    // The path prompt stores its horizontal position in pixels; remeasure it
    // against the new font on the next render.
    m_pathPromptScrollPx = 0;
}

void DrawingApp::invalidateHitTargets() {
    m_hitTargetsValid = false;
    m_btnNew = {0, 0, 0, 0};
    m_btnSave = {0, 0, 0, 0};
    m_btnOpen = {0, 0, 0, 0};
    m_btnUndo = {0, 0, 0, 0};
    m_btnRedo = {0, 0, 0, 0};
    m_btnPen = {0, 0, 0, 0};
    m_btnEraser = {0, 0, 0, 0};
    m_btnFill = {0, 0, 0, 0};
    m_btnEyedropper = {0, 0, 0, 0};
    m_btnLine = {0, 0, 0, 0};
    m_btnRect = {0, 0, 0, 0};
    m_btnRgb = {0, 0, 0, 0};
    m_btnClear = {0, 0, 0, 0};
    m_btnBrushSmall = {0, 0, 0, 0};
    m_btnBrushMedium = {0, 0, 0, 0};
    m_btnBrushLarge = {0, 0, 0, 0};
    for (SDL_Rect& swatch : m_colorSwatches) {
        swatch = {0, 0, 0, 0};
    }
}

void DrawingApp::ensureHitTargets() {
    if (m_hitTargetsValid) return;

    const int buttonHeight = getToolbarButtonHeight();
    auto placeButton = [&](SDL_Rect& rect, int width, int& x, int y) {
        rect = {x, y, width, buttonHeight};
        x += width + kToolbarGap;
    };

    int x = kToolbarPadding;
    placeButton(m_btnNew, 38, x, kToolbarPadding);
    placeButton(m_btnSave, 42, x, kToolbarPadding);
    placeButton(m_btnOpen, 44, x, kToolbarPadding);
    x += 4;
    placeButton(m_btnUndo, 44, x, kToolbarPadding);
    placeButton(m_btnRedo, 42, x, kToolbarPadding);

    x = kToolbarPadding;
    const int toolRowY = kToolbarPadding + buttonHeight + 6;
    placeButton(m_btnPen, 44, x, toolRowY);
    placeButton(m_btnEraser, 54, x, toolRowY);
    placeButton(m_btnFill, 40, x, toolRowY);
    placeButton(m_btnEyedropper, 42, x, toolRowY);
    placeButton(m_btnLine, 42, x, toolRowY);
    placeButton(m_btnRect, 42, x, toolRowY);
    placeButton(m_btnClear, 48, x, toolRowY);
    x += 4;
    placeButton(m_btnBrushSmall, 24, x, toolRowY);
    placeButton(m_btnBrushMedium, 24, x, toolRowY);
    placeButton(m_btnBrushLarge, 24, x, toolRowY);

    x = kToolbarPadding;
    const int colorRowY = toolRowY + buttonHeight + 6;
    placeButton(m_btnRgb, 42, x, colorRowY);
    x += 6;
    for (SDL_Rect& swatch : m_colorSwatches) {
        swatch = {x, colorRowY + 2, kSwatchSize, kSwatchSize};
        x += kSwatchSize + kSwatchGap;
    }

    m_hitTargetsValid = true;
}

void DrawingApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (m_clientWidth != contentRect.w || m_clientHeight != contentRect.h) {
        onResize(contentRect.w, contentRect.h);
    }
    ensureHitTargets();

    drawToolbar(renderer, contentRect);

    SDL_Rect canvasRect = {
        contentRect.x,
        contentRect.y + m_canvasTop,
        contentRect.w,
        std::max(0, contentRect.h - m_canvasTop - m_statusBarHeight)
    };

    SDL_SetRenderDrawColor(renderer, kCanvasBackgroundR, kCanvasBackgroundG, kCanvasBackgroundB, 255);
    SDL_RenderFillRect(renderer, &canvasRect);

    syncTexture(renderer);
    if (m_canvasTexture) {
        SDL_RenderCopy(renderer, m_canvasTexture, nullptr, &canvasRect);
    }

    SDL_SetRenderDrawColor(renderer, 55, 58, 66, 255);
    SDL_RenderDrawRect(renderer, &canvasRect);

    drawStatusBar(renderer, contentRect);
}

void DrawingApp::handleEvent(const SDL_Event& event) {
    if (m_pathPromptMode != PathPromptMode::None) {
        if (event.type == SDL_KEYDOWN) {
            handlePathPromptKey(event.key.keysym);
        } else if (event.type == SDL_TEXTINPUT) {
            handlePathPromptText(event.text.text);
        }
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        const SDL_Keysym& key = event.key.keysym;
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_s) {
            if (m_filePath.empty()) {
                beginPathPrompt(PathPromptMode::Save);
            } else {
                saveToPath(m_filePath);
            }
            return;
        }
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_o) {
            beginPathPrompt(PathPromptMode::Open);
            return;
        }
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_z) {
            if (key.mod & KMOD_SHIFT) {
                redoCanvas();
            } else {
                undoCanvas();
            }
            return;
        }
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_y) {
            redoCanvas();
            return;
        }
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_n) {
            startNewSketch();
            return;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x;
        const int y = event.button.y;

        if (y < m_canvasTop) {
            handleToolbarClick(x, y);
            return;
        }

        if (isInCanvas(x, y)) {
            int cx = 0;
            int cy = 0;
            canvasPointFromClient(x, y, cx, cy);
            if (m_tool == Tool::Eyedropper) {
                pickColorAt(cx, cy);
                return;
            }
            if (m_tool == Tool::Fill) {
                floodFill(cx, cy);
                return;
            }
            m_drawing = true;
            m_lastCanvasX = cx;
            m_lastCanvasY = cy;
            m_shapeAnchorX = cx;
            m_shapeAnchorY = cy;
            beginSparseHistory();
            if (m_tool == Tool::Line || m_tool == Tool::Rect) {
                return;
            }
            if (stampBrush(cx, cy)) {
                recordSparseHistoryChange();
                m_dirty = true;
                clearDiscardArm();
            }
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (m_drawing && (m_tool == Tool::Line || m_tool == Tool::Rect)
            && m_shapeAnchorX >= 0 && m_shapeAnchorY >= 0) {
            int cx = 0;
            int cy = 0;
            canvasPointFromClient(event.button.x, event.button.y, cx, cy);
            if (commitShape(m_shapeAnchorX, m_shapeAnchorY, cx, cy)) {
                recordSparseHistoryChange();
                m_dirty = true;
                clearDiscardArm();
            }
        }
        m_drawing = false;
        m_lastCanvasX = -1;
        m_lastCanvasY = -1;
        m_shapeAnchorX = -1;
        m_shapeAnchorY = -1;
        finishSparseHistory();
        return;
    }

    if (event.type == SDL_MOUSEMOTION && m_drawing && m_tool != Tool::Fill) {
        int cx = 0;
        int cy = 0;
        canvasPointFromClient(event.motion.x, event.motion.y, cx, cy);

        if (m_tool == Tool::Line || m_tool == Tool::Rect) {
            m_lastCanvasX = cx;
            m_lastCanvasY = cy;
            return;
        }

        if (m_lastCanvasX >= 0 && m_lastCanvasY >= 0) {
            if (drawStroke(m_lastCanvasX, m_lastCanvasY, cx, cy)) {
                recordSparseHistoryChange();
                m_dirty = true;
                clearDiscardArm();
            }
        } else if (stampBrush(cx, cy)) {
            recordSparseHistoryChange();
            m_dirty = true;
            clearDiscardArm();
        }

        m_lastCanvasX = cx;
        m_lastCanvasY = cy;
    }
}

} // namespace monolith::app
