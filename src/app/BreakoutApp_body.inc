void BreakoutApp::drawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color,
                           const SDL_Rect* clip) const {
    if (!m_font || !text || !*text) return;
    const auto cached = m_textTextureCache.get(renderer, m_font, text, color);
    if (!cached) return;
    SDL_Rect dst{x, y, cached.width, cached.height};
    if (clip) {
        const RendererClipState previousClip = captureRendererClip(renderer);
        const SDL_Rect effectiveClip = intersectRendererClip(*clip, previousClip);
        if (effectiveClip.w > 0 && effectiveClip.h > 0) {
            SDL_RenderSetClipRect(renderer, &effectiveClip);
            SDL_RenderCopy(renderer, cached.handle, nullptr, &dst);
        }
        restoreRendererClip(renderer, previousClip);
    } else {
        SDL_RenderCopy(renderer, cached.handle, nullptr, &dst);
    }
}

void BreakoutApp::drawCentered(SDL_Renderer* renderer, const char* text, const SDL_Rect& area,
                               SDL_Color color) const {
    if (!m_font || !text || !*text) return;
    const auto cached = m_textTextureCache.get(renderer, m_font, text, color);
    if (cached) {
        SDL_Rect dst{
            area.x + (area.w - cached.width) / 2,
            area.y + (area.h - cached.height) / 2,
            cached.width,
            cached.height
        };
        SDL_RenderCopy(renderer, cached.handle, nullptr, &dst);
    }
}

void BreakoutApp::fieldToScreen(const SDL_Rect& contentRect, float fx, float fy, int fw, int fh,
                                SDL_Rect& out) const {
    const int availW = contentRect.w;
    const int hudH = hudHeight();
    const int availH = std::max(1, contentRect.h - hudH);
    const float scale = std::min(
        static_cast<float>(availW) / static_cast<float>(monolith::breakout::Game::kFieldW),
        static_cast<float>(availH) / static_cast<float>(monolith::breakout::Game::kFieldH));
    const int boardW = static_cast<int>(monolith::breakout::Game::kFieldW * scale);
    const int boardH = static_cast<int>(monolith::breakout::Game::kFieldH * scale);
    const int boardX = contentRect.x + (availW - boardW) / 2;
    const int boardY = contentRect.y + hudH + (availH - boardH) / 2;
    out.x = boardX + static_cast<int>(fx * scale);
    out.y = boardY + static_cast<int>(fy * scale);
    out.w = std::max(1, static_cast<int>(static_cast<float>(fw) * scale));
    out.h = std::max(1, static_cast<int>(static_cast<float>(fh) * scale));
}

void BreakoutApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (m_clientWidth != contentRect.w || m_clientHeight != contentRect.h) {
        onResize(contentRect.w, contentRect.h);
    }

    SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
    SDL_RenderFillRect(renderer, &contentRect);
    const int hudH = hudHeight();
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;

    char hud[80];
    std::snprintf(hud, sizeof(hud), "Score %d   Lives %d   Bricks %d",
                  m_game.score, m_game.lives, m_game.bricksLeft);
    const SDL_Rect hudClip = {contentRect.x, contentRect.y, contentRect.w, hudH};
    drawText(renderer, hud, contentRect.x + 10,
             contentRect.y + std::max(0, (hudH - fontHeight) / 2), kHud, &hudClip);

    SDL_Rect field;
    fieldToScreen(contentRect, 0, 0, monolith::breakout::Game::kFieldW,
                  monolith::breakout::Game::kFieldH, field);
    SDL_SetRenderDrawColor(renderer, 28, 32, 42, 255);
    SDL_RenderFillRect(renderer, &field);
    SDL_SetRenderDrawColor(renderer, 70, 78, 92, 255);
    SDL_RenderDrawRect(renderer, &field);

    for (int row = 0; row < monolith::breakout::Game::kBrickRows; ++row) {
        for (int col = 0; col < monolith::breakout::Game::kBrickCols; ++col) {
            if (!m_game.brickAlive(col, row)) continue;
            float bx, by, bw, bh;
            m_game.brickRect(col, row, bx, by, bw, bh);
            SDL_Rect brick;
            fieldToScreen(contentRect, bx, by, static_cast<int>(bw), static_cast<int>(bh), brick);
            const SDL_Color c = brickColor(row);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRect(renderer, &brick);
        }
    }

    const float paddleY = static_cast<float>(
        monolith::breakout::Game::kFieldH - 20 - monolith::breakout::Game::kPaddleH);
    SDL_Rect paddle;
    fieldToScreen(contentRect, m_game.paddleX, paddleY,
                  monolith::breakout::Game::kPaddleW, monolith::breakout::Game::kPaddleH, paddle);
    SDL_SetRenderDrawColor(renderer, 90, 180, 230, 255);
    SDL_RenderFillRect(renderer, &paddle);

    SDL_Rect ball;
    fieldToScreen(contentRect, m_game.ballX, m_game.ballY,
                  monolith::breakout::Game::kBallSize, monolith::breakout::Game::kBallSize, ball);
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
    SDL_RenderFillRect(renderer, &ball);

    if (m_paused && m_game.state == monolith::breakout::State::Playing) {
        drawCentered(renderer, "Paused - Space/click to resume", field, kDim);
    } else if (m_game.state == monolith::breakout::State::Won) {
        drawCentered(renderer, "You win - R or Enter", field, kOverlay);
    } else if (m_game.state == monolith::breakout::State::GameOver) {
        drawCentered(renderer, "Game over - R or Enter", field, kOverlay);
    }
}
