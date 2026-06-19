#include "DrawingApp.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace monolith::app {

namespace {
constexpr int kToolbarPadding = 8;
constexpr int kToolbarButtonHeight = 22;
constexpr int kToolbarGap = 6;
constexpr int kSwatchSize = 18;
constexpr int kSwatchGap = 4;
constexpr uint8_t kCanvasBackgroundR = 245;
constexpr uint8_t kCanvasBackgroundG = 245;
constexpr uint8_t kCanvasBackgroundB = 248;

constexpr char kModrMagic[4] = {'M', 'O', 'D', 'R'};

bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void writeU32LE(std::string& out, uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
}

uint32_t readU32LE(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data() + offset);
    return static_cast<uint32_t>(bytes[0])
         | (static_cast<uint32_t>(bytes[1]) << 8)
         | (static_cast<uint32_t>(bytes[2]) << 16)
         | (static_cast<uint32_t>(bytes[3]) << 24);
}
} // namespace

DrawingApp::DrawingApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    setStatus("Pen ready. Drag on the canvas to draw. Ctrl+S save, Ctrl+O open.");
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

void DrawingApp::clearCanvas() {
    for (size_t i = 0; i < m_pixels.size(); i += 4) {
        m_pixels[i + 0] = kCanvasBackgroundR;
        m_pixels[i + 1] = kCanvasBackgroundG;
        m_pixels[i + 2] = kCanvasBackgroundB;
        m_pixels[i + 3] = 255;
    }
    m_dirty = true;
    markTextureDirty();
    setStatus("Canvas cleared.");
}

void DrawingApp::markTextureDirty() {
    m_textureDirty = true;
}

void DrawingApp::syncTexture(SDL_Renderer* renderer) {
    if (!renderer || m_canvasWidth <= 0 || m_canvasHeight <= 0) return;

    if (!m_canvasTexture
        || m_textureDirty) {
        if (m_canvasTexture) {
            SDL_DestroyTexture(m_canvasTexture);
            m_canvasTexture = nullptr;
        }

        m_canvasTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            m_canvasWidth,
            m_canvasHeight
        );

        if (!m_canvasTexture) return;
        m_textureDirty = false;
    }

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
    return kColors[m_colorIndex].r;
}

uint8_t DrawingApp::activeGreen() const {
    if (m_tool == Tool::Eraser) return kCanvasBackgroundG;
    return kColors[m_colorIndex].g;
}

uint8_t DrawingApp::activeBlue() const {
    if (m_tool == Tool::Eraser) return kCanvasBackgroundB;
    return kColors[m_colorIndex].b;
}

void DrawingApp::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= m_canvasWidth || y >= m_canvasHeight) return;
    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(m_canvasWidth) + static_cast<size_t>(x)) * 4;
    m_pixels[idx + 0] = r;
    m_pixels[idx + 1] = g;
    m_pixels[idx + 2] = b;
    m_pixels[idx + 3] = 255;
}

void DrawingApp::stampBrush(int x, int y) {
    const int radius = brushRadius();
    const uint8_t r = activeRed();
    const uint8_t g = activeGreen();
    const uint8_t b = activeBlue();
    const int r2 = radius * radius;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                setPixel(x + dx, y + dy, r, g, b);
            }
        }
    }
}

void DrawingApp::drawStroke(int x0, int y0, int x1, int y1) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    while (true) {
        stampBrush(x, y);

        if (x == x1 && y == y1) break;

        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }

    m_dirty = true;
    markTextureDirty();
}

bool DrawingApp::isInCanvas(int x, int y) const {
    return x >= 0 && y >= m_canvasTop
        && x < m_clientWidth
        && y < m_clientHeight - m_statusBarHeight;
}

void DrawingApp::canvasPointFromClient(int clientX, int clientY, int& outX, int& outY) const {
    outX = clientX;
    outY = clientY - m_canvasTop;
}

std::string DrawingApp::defaultSavePath() {
    if (!m_fs) return "/home/monolith/drawings/sketch.modr";

    const std::string baseDir = "/home/monolith/drawings";
    m_fs->createDirectory("/home/monolith");
    m_fs->createDirectory(baseDir);

    for (int i = 1; i < 1000; ++i) {
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
        setStatus("Save failed: filesystem not available.");
        return false;
    }
    if (m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        setStatus("Save failed: canvas is empty.");
        return false;
    }

    std::string path = m_fs->normalize(virtualPath);
    if (!path.ends_with(".modr")) {
        path += ".modr";
    }

    std::string parent = path;
    const size_t slash = parent.find_last_of('/');
    if (slash != std::string::npos) {
        parent = parent.substr(0, slash);
        if (!parent.empty()) {
            m_fs->createDirectory(parent);
        }
    }

    std::string blob;
    blob.reserve(12 + static_cast<size_t>(m_canvasWidth) * static_cast<size_t>(m_canvasHeight) * 3);
    blob.append(kModrMagic, 4);
    writeU32LE(blob, static_cast<uint32_t>(m_canvasWidth));
    writeU32LE(blob, static_cast<uint32_t>(m_canvasHeight));

    for (int y = 0; y < m_canvasHeight; ++y) {
        for (int x = 0; x < m_canvasWidth; ++x) {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(m_canvasWidth) + static_cast<size_t>(x)) * 4;
            blob.push_back(static_cast<char>(m_pixels[idx + 0]));
            blob.push_back(static_cast<char>(m_pixels[idx + 1]));
            blob.push_back(static_cast<char>(m_pixels[idx + 2]));
        }
    }

    if (!m_fs->writeFile(path, blob)) {
        setStatus("Save failed: could not write file.");
        return false;
    }

    m_filePath = path;
    m_dirty = false;

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
    const std::string blob = m_fs->readFile(path);
    if (blob.size() < 12) {
        setStatus("Open failed: file is too small or missing.");
        return false;
    }
    if (std::memcmp(blob.data(), kModrMagic, 4) != 0) {
        setStatus("Open failed: not a .modr drawing file.");
        return false;
    }

    const uint32_t width = readU32LE(blob, 4);
    const uint32_t height = readU32LE(blob, 8);
    if (width == 0 || height == 0 || width > 4096 || height > 4096) {
        setStatus("Open failed: invalid canvas dimensions.");
        return false;
    }

    const size_t expectedPixels = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    if (blob.size() != 12 + expectedPixels) {
        setStatus("Open failed: corrupt .modr file.");
        return false;
    }

    resizeCanvas(static_cast<int>(width), static_cast<int>(height), false);
    size_t offset = 12;
    for (int y = 0; y < static_cast<int>(height); ++y) {
        for (int x = 0; x < static_cast<int>(width); ++x) {
            const uint8_t r = static_cast<uint8_t>(blob[offset++]);
            const uint8_t g = static_cast<uint8_t>(blob[offset++]);
            const uint8_t b = static_cast<uint8_t>(blob[offset++]);
            setPixel(x, y, r, g, b);
        }
    }

    m_filePath = path;
    m_dirty = false;
    markTextureDirty();

    size_t nameStart = path.find_last_of('/');
    const std::string baseName = (nameStart != std::string::npos) ? path.substr(nameStart + 1) : path;
    if (auto* ctrl = getController()) {
        ctrl->setTitle("Drawing - " + baseName);
    }

    setStatus("Opened: " + path);
    return true;
}

void DrawingApp::setStatus(const std::string& message) {
    m_statusMessage = message;
}

void DrawingApp::beginPathPrompt(PathPromptMode mode) {
    m_pathPromptMode = mode;
    if (mode == PathPromptMode::Save) {
        m_pathPromptBuffer = m_filePath.empty() ? defaultSavePath() : m_filePath;
        setStatus("Save as (Enter confirm, Esc cancel):");
    } else if (mode == PathPromptMode::Open) {
        m_pathPromptBuffer = m_filePath.empty() ? "/home/monolith/drawings/" : m_filePath;
        setStatus("Open path (Enter confirm, Esc cancel):");
    }
}

void DrawingApp::finishPathPrompt(bool commit) {
    const PathPromptMode mode = m_pathPromptMode;
    const std::string buffer = m_pathPromptBuffer;
    m_pathPromptMode = PathPromptMode::None;
    m_pathPromptBuffer.clear();

    if (!commit) {
        setStatus("Cancelled.");
        return;
    }

    if (buffer.empty()) {
        setStatus("Path cannot be empty.");
        return;
    }

    if (mode == PathPromptMode::Save) {
        saveToPath(buffer);
    } else if (mode == PathPromptMode::Open) {
        loadFromPath(buffer);
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
            if (!m_pathPromptBuffer.empty()) {
                m_pathPromptBuffer.pop_back();
            }
            break;
        default:
            break;
    }
}

void DrawingApp::handlePathPromptText(const char* text) {
    if (!text) return;
    for (const char* p = text; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 32 && c < 127) {
            m_pathPromptBuffer.push_back(static_cast<char>(c));
        }
    }
}

void DrawingApp::handleToolbarClick(int x, int y) {
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
            m_tool = Tool::Pen;
            setStatus(std::string("Color: ") + kColors[i].name);
            return;
        }
    }
}

void DrawingApp::drawToolbar(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    SDL_Rect toolbar = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        m_canvasTop
    };
    SDL_SetRenderDrawColor(renderer, 32, 34, 40, 255);
    SDL_RenderFillRect(renderer, &toolbar);

    int relX = kToolbarPadding;
    const int relY = kToolbarPadding;

    auto drawButton = [&](SDL_Rect& outRect, const char* label, int width, bool active) {
        outRect = {relX, relY, width, kToolbarButtonHeight};
        SDL_Rect drawRect = {
            contentRect.x + relX,
            contentRect.y + relY,
            width,
            kToolbarButtonHeight
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
            SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, label, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect dst = {
                        drawRect.x + (drawRect.w - surf->w) / 2,
                        drawRect.y + (drawRect.h - surf->h) / 2,
                        surf->w,
                        surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        relX += width + kToolbarGap;
    };

    drawButton(m_btnPen, "Pen", 44, m_tool == Tool::Pen);
    drawButton(m_btnEraser, "Eraser", 54, m_tool == Tool::Eraser);
    drawButton(m_btnClear, "Clear", 48, false);

    relX += 4;
    drawButton(m_btnBrushSmall, "S", 24, m_brush == BrushSize::Small);
    drawButton(m_btnBrushMedium, "M", 24, m_brush == BrushSize::Medium);
    drawButton(m_btnBrushLarge, "L", 24, m_brush == BrushSize::Large);

    relX += 6;
    for (int i = 0; i < kColorCount; ++i) {
        m_colorSwatches[i] = {relX, relY + 2, kSwatchSize, kSwatchSize};
        SDL_Rect swatch = {
            contentRect.x + relX,
            contentRect.y + relY + 2,
            kSwatchSize,
            kSwatchSize
        };
        SDL_SetRenderDrawColor(renderer, kColors[i].r, kColors[i].g, kColors[i].b, 255);
        SDL_RenderFillRect(renderer, &swatch);

        if (i == m_colorIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &swatch);
            SDL_Rect inner = {swatch.x + 1, swatch.y + 1, swatch.w - 2, swatch.h - 2};
            SDL_RenderDrawRect(renderer, &inner);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 64, 72, 255);
            SDL_RenderDrawRect(renderer, &swatch);
        }

        relX += kSwatchSize + kSwatchGap;
    }
}

void DrawingApp::drawStatusBar(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
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
    if (m_pathPromptMode != PathPromptMode::None) {
        text += " " + m_pathPromptBuffer + "_";
    } else if (m_dirty) {
        text += "  [modified]";
    }

    SDL_Color col = {170, 175, 185, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, text.c_str(), col);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_Rect dst = {
                contentRect.x + 8,
                bar.y + (bar.h - surf->h) / 2,
                std::min(surf->w, bar.w - 16),
                surf->h
            };
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void DrawingApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;

    const int canvasW = std::max(1, clientWidth);
    const int canvasH = std::max(1, clientHeight - m_canvasTop - m_statusBarHeight);
    resizeCanvas(canvasW, canvasH, true);
}

void DrawingApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (m_clientWidth <= 0 || m_clientHeight <= 0) {
        m_clientWidth = contentRect.w;
        m_clientHeight = contentRect.h;
        onResize(m_clientWidth, m_clientHeight);
    }

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
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_n) {
            clearCanvas();
            m_filePath.clear();
            if (auto* ctrl = getController()) {
                ctrl->setTitle("Drawing");
            }
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
            m_drawing = true;
            m_lastCanvasX = cx;
            m_lastCanvasY = cy;
            stampBrush(cx, cy);
            m_dirty = true;
            markTextureDirty();
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        m_drawing = false;
        m_lastCanvasX = -1;
        m_lastCanvasY = -1;
        return;
    }

    if (event.type == SDL_MOUSEMOTION && m_drawing) {
        const int x = event.motion.x;
        const int y = event.motion.y;
        if (!isInCanvas(x, y)) return;

        int cx = 0;
        int cy = 0;
        canvasPointFromClient(x, y, cx, cy);

        if (m_lastCanvasX >= 0 && m_lastCanvasY >= 0) {
            drawStroke(m_lastCanvasX, m_lastCanvasY, cx, cy);
        } else {
            stampBrush(cx, cy);
            m_dirty = true;
            markTextureDirty();
        }

        m_lastCanvasX = cx;
        m_lastCanvasY = cy;
    }
}

} // namespace monolith::app