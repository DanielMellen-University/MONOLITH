void WindowManager::launchByAction(AppAction action, const std::string& path) {
    switch (action) {
        case AppAction::Terminal: launchTerminal(); break;
        case AppAction::TextEditor: launchTextEditor(path); break;
        case AppAction::Filesystem: launchFilesystem(); break;
        case AppAction::Settings: launchSettings(); break;
        case AppAction::Drawing: launchDrawing(path); break;
        case AppAction::Snake: launchSnake(); break;
        case AppAction::Minesweeper: launchMinesweeper(); break;
        case AppAction::Pong: launchPong(); break;
        case AppAction::Breakout: launchBreakout(); break;
        case AppAction::ShutDown: requestQuit(); break;
        default: break;
    }
}

void WindowManager::activateStartMenuItem(int action) {
    // Launch while the menu is still modal. The new window becomes focused
    // without waking the suspended app, then closing the menu resumes the
    // actual focused window exactly once.
    launchByAction(static_cast<AppAction>(action));
    setStartMenuVisible(false);
}
