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

