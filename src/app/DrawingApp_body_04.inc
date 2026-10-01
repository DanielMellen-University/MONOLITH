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
                markTextureDirty();
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
                markTextureDirty();
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
                markTextureDirty();
            }
        } else if (stampBrush(cx, cy)) {
            recordSparseHistoryChange();
            m_dirty = true;
            clearDiscardArm();
            markTextureDirty();
        }

        m_lastCanvasX = cx;
        m_lastCanvasY = cy;
    }
}

} // namespace monolith::app
