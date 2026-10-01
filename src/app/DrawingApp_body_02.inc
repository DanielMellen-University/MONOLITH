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
    markTextureDirty();
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
    if (m_canvasWidth > monolith::drawing::kMaxCanvasDimension
        || m_canvasHeight > monolith::drawing::kMaxCanvasDimension) {
        clearDiscardArm();
        setStatus("Save failed: exceeds 4096×4096 canvas limit.");
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

    std::uint64_t fileBytes = 0;
    if (!m_fs->fileSize(path, fileBytes)) {
        setStatus("Open failed: could not read file.");
        return false;
    }
    if (fileBytes > monolith::drawing::maxModrFileBytes()) {
        setStatus("Open failed: exceeds 4096×4096 .modr limit.");
        return false;
    }

    std::string blob;
    blob.reserve(static_cast<std::size_t>(fileBytes));
    bool overflow = false;
    const bool readSucceeded = m_fs->readFileChunks(path, [&](std::string_view chunk) {
        if (chunk.size() > monolith::drawing::maxModrFileBytes() - blob.size()) {
            overflow = true;
            return false;
        }
        blob.append(chunk.data(), chunk.size());
        return true;
    });
    if (overflow) {
        setStatus("Open failed: exceeds 4096×4096 .modr limit.");
        return false;
    }
    if (!readSucceeded) {
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

