#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <iostream>
#include <memory>

namespace {

class CaptureApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onFocusGained() override { ++focusGained; }
    void onFocusLost() override { ++focusLost; }

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            ++downs;
            downX = event.button.x;
            downY = event.button.y;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            ++ups;
            upX = event.button.x;
            upY = event.button.y;
        } else if (event.type == SDL_MOUSEMOTION) {
            ++motions;
            motionX = event.motion.x;
            motionY = event.motion.y;
        } else if (event.type == SDL_MOUSEWHEEL) {
            ++wheelEvents;
        } else if (event.type == SDL_KEYUP) {
            ++keyUps;
        } else if (event.type == SDL_KEYDOWN) {
            ++keyDowns;
        }
    }

    int downs = 0;
    int ups = 0;
    int motions = 0;
    int wheelEvents = 0;
    int keyUps = 0;
    int keyDowns = 0;
    int focusGained = 0;
    int focusLost = 0;
    int downX = -1;
    int downY = -1;
    int upX = -1;
    int upY = -1;
    int motionX = -1;
    int motionY = -1;
};

void leftButton(SDL_Event& event, Uint32 type, int x, int y) {
    event = {};
    event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
}

} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "FAIL: SDL video initialize: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* hostWindow = SDL_CreateWindow(
        "window mouse capture test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1000, 700, SDL_WINDOW_SHOWN);
    if (!hostWindow) {
        std::cerr << "FAIL: SDL host window create: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_ShowWindow(hostWindow);
    SDL_RaiseWindow(hostWindow);
    SDL_SetWindowInputFocus(hostWindow);

    monolith::window::WindowManager wm;
    wm.setLogicalDesktopSize(1000, 700);

    auto first = std::make_unique<CaptureApp>();
    CaptureApp* firstPtr = first.get();
    auto* firstWindow = wm.createWindow("First", 100, 100, 300, 240, std::move(first));

    auto second = std::make_unique<CaptureApp>();
    CaptureApp* secondPtr = second.get();
    wm.createWindow("Second", 500, 100, 300, 240, std::move(second));

    SDL_Event down{};
    leftButton(down, SDL_MOUSEBUTTONDOWN, 120, 150);
    wm.handleEvent(down);
    check(firstPtr->downs == 1 && firstPtr->downX == 20 && firstPtr->downY == 18,
          "first client receives translated button down");

    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 900;
    motion.motion.y = 650;
    wm.handleEvent(motion);
    check(firstPtr->motions == 1 && firstPtr->motionX == 800 && firstPtr->motionY == 518,
          "captured client receives motion outside its window");

    SDL_Event secondDown{};
    leftButton(secondDown, SDL_MOUSEBUTTONDOWN, 520, 150);
    wm.handleEvent(secondDown);
    check(secondPtr->downs == 1, "focus can change while the button is held");

    SDL_Event up{};
    leftButton(up, SDL_MOUSEBUTTONUP, 900, 650);
    wm.handleEvent(up);
    check(firstPtr->ups == 1 && firstPtr->upX == 800 && firstPtr->upY == 518,
          "release returns to the original client after focus changes");
    check(secondPtr->ups == 0, "focused replacement does not steal the release");

    SDL_Event wheel{};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.y = 1;
    wm.handleEvent(wheel);
    check(firstPtr->wheelEvents == 0 && secondPtr->wheelEvents == 0,
          "wheel routing uses the latest button-up pointer position");

    SDL_WarpMouseInWindow(hostWindow, 520, 150);
    SDL_PumpEvents();
    SDL_Event stationaryWheel{};
    stationaryWheel.type = SDL_MOUSEWHEEL;
    stationaryWheel.wheel.y = 1;
    wm.handleEvent(stationaryWheel);
    check(firstPtr->wheelEvents == 0 && secondPtr->wheelEvents == 1,
          "wheel routing refreshes the host pointer position");

    SDL_Event taskbarDown{};
    leftButton(taskbarDown, SDL_MOUSEBUTTONDOWN, 50, 680);
    wm.handleEvent(taskbarDown);
    SDL_Event taskbarMotion{};
    taskbarMotion.type = SDL_MOUSEMOTION;
    taskbarMotion.motion.x = 520;
    taskbarMotion.motion.y = 150;
    const int motionsBeforeTaskbarCapture = secondPtr->motions;
    wm.handleEvent(taskbarMotion);
    SDL_Event taskbarUp{};
    leftButton(taskbarUp, SDL_MOUSEBUTTONUP, 50, 680);
    wm.handleEvent(taskbarUp);
    check(firstPtr->ups == 1 && secondPtr->ups == 0,
          "taskbar clicks do not leak a mouse-up into a client app");
    check(secondPtr->motions == motionsBeforeTaskbarCapture,
          "taskbar clicks do not leak motion into a client app");

    SDL_Event desktopDown{};
    leftButton(desktopDown, SDL_MOUSEBUTTONDOWN, 900, 650);
    wm.handleEvent(desktopDown);
    SDL_Event desktopMotion{};
    desktopMotion.type = SDL_MOUSEMOTION;
    desktopMotion.motion.x = 520;
    desktopMotion.motion.y = 150;
    const int motionsBeforeDesktopCapture = secondPtr->motions;
    wm.handleEvent(desktopMotion);
    SDL_Event desktopUp{};
    leftButton(desktopUp, SDL_MOUSEBUTTONUP, 900, 650);
    wm.handleEvent(desktopUp);
    check(secondPtr->ups == 0,
          "empty-desktop clicks do not leak a mouse-up into the focused client");
    check(secondPtr->motions == motionsBeforeDesktopCapture,
          "empty-desktop clicks do not leak motion into the focused client");

    SDL_Event frameDragDown{};
    leftButton(frameDragDown, SDL_MOUSEBUTTONDOWN, 150, 110);
    wm.handleEvent(frameDragDown);
    SDL_Event frameMotion{};
    frameMotion.type = SDL_MOUSEMOTION;
    frameMotion.motion.x = 200;
    frameMotion.motion.y = 150;
    const int motionsBeforeFrameDrag = firstPtr->motions;
    wm.handleEvent(frameMotion);
    check(firstPtr->motions == motionsBeforeFrameDrag,
          "window-frame drags do not leak motion into the client");
    SDL_Event frameDragUp{};
    leftButton(frameDragUp, SDL_MOUSEBUTTONUP, 200, 150);
    wm.handleEvent(frameDragUp);

    const int firstFocusGainedBeforeHostLoss = firstPtr->focusGained;
    const int firstFocusLostBeforeHostLoss = firstPtr->focusLost;
    SDL_Event clientDownAfterFrame{};
    leftButton(clientDownAfterFrame, SDL_MOUSEBUTTONDOWN,
               firstWindow->rect.x + 12,
               firstWindow->rect.y + monolith::window::Window::TITLE_BAR_HEIGHT + 12);
    wm.handleEvent(clientDownAfterFrame);
    SDL_WarpMouseInWindow(hostWindow,
                          firstWindow->rect.x + 12,
                          firstWindow->rect.y + monolith::window::Window::TITLE_BAR_HEIGHT + 12);
    SDL_PumpEvents();
    const int firstUpsBeforeHostLoss = firstPtr->ups;
    const int firstDownsBeforeHostLoss = firstPtr->downs;
    const int firstMotionsBeforeHostLoss = firstPtr->motions;
    const int firstWheelBeforeHostLoss = firstPtr->wheelEvents;
    SDL_Event focusLost{};
    focusLost.type = SDL_WINDOWEVENT;
    focusLost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    focusLost.window.windowID = 1;
    wm.handleEvent(focusLost);
    check(firstPtr->ups == firstUpsBeforeHostLoss + 1,
          "host focus loss synthesizes the captured client release");
    check(firstPtr->focusLost == firstFocusLostBeforeHostLoss + 1,
          "host focus loss notifies the focused client");

    SDL_Event blockedMotion{};
    blockedMotion.type = SDL_MOUSEMOTION;
    blockedMotion.motion.x = 120;
    blockedMotion.motion.y = 150;
    wm.handleEvent(blockedMotion);
    SDL_Event blockedDown{};
    leftButton(blockedDown, SDL_MOUSEBUTTONDOWN, 120, 150);
    wm.handleEvent(blockedDown);
    SDL_Event blockedUp = blockedDown;
    blockedUp.type = SDL_MOUSEBUTTONUP;
    wm.handleEvent(blockedUp);
    SDL_Event blockedWheel{};
    blockedWheel.type = SDL_MOUSEWHEEL;
    blockedWheel.wheel.y = 1;
    wm.handleEvent(blockedWheel);
    check(firstPtr->downs == firstDownsBeforeHostLoss
              && firstPtr->motions == firstMotionsBeforeHostLoss
              && firstPtr->wheelEvents == firstWheelBeforeHostLoss,
          "host-unfocused pointer input does not reach the shell or app");

    monolith::window::Window* focusedBeforeUnfocusedHotkeys = wm.m_focusedWindow;
    const bool startMenuBeforeUnfocusedHotkeys = wm.m_showStartMenu;
    SDL_Event blockedCtrlEscape{};
    blockedCtrlEscape.type = SDL_KEYDOWN;
    blockedCtrlEscape.key.keysym.sym = SDLK_ESCAPE;
    blockedCtrlEscape.key.keysym.mod = KMOD_CTRL;
    wm.handleEvent(blockedCtrlEscape);
    SDL_Event blockedCtrlEscapeRelease = blockedCtrlEscape;
    blockedCtrlEscapeRelease.type = SDL_KEYUP;
    wm.handleEvent(blockedCtrlEscapeRelease);
    SDL_Event blockedAltTab{};
    blockedAltTab.type = SDL_KEYDOWN;
    blockedAltTab.key.keysym.sym = SDLK_TAB;
    blockedAltTab.key.keysym.mod = KMOD_ALT;
    wm.handleEvent(blockedAltTab);
    SDL_Event blockedAltTabRelease = blockedAltTab;
    blockedAltTabRelease.type = SDL_KEYUP;
    wm.handleEvent(blockedAltTabRelease);
    check(wm.m_focusedWindow == focusedBeforeUnfocusedHotkeys
              && wm.m_showStartMenu == startMenuBeforeUnfocusedHotkeys,
          "host-unfocused shell hotkeys do not change focus or open Start");

    SDL_Event focusGained{};
    focusGained.type = SDL_WINDOWEVENT;
    focusGained.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    focusGained.window.windowID = 1;
    wm.handleEvent(focusGained);
    check(firstPtr->focusGained == firstFocusGainedBeforeHostLoss + 1,
          "host focus gain notifies the focused client");

    const int closeX = firstWindow->rect.x + firstWindow->rect.w - 10 - 16 + 4;
    const int closeY = firstWindow->rect.y + (monolith::window::Window::TITLE_BAR_HEIGHT - 16) / 2 + 4;
    SDL_Event frameDown{};
    leftButton(frameDown, SDL_MOUSEBUTTONDOWN, closeX, closeY);
    wm.handleEvent(frameDown);
    const int secondUpsBeforeFrameRelease = secondPtr->ups;
    SDL_Event frameUp{};
    leftButton(frameUp, SDL_MOUSEBUTTONUP, closeX, closeY);
    wm.handleEvent(frameUp);
    check(secondPtr->ups == secondUpsBeforeFrameRelease,
          "window-frame clicks do not leak a mouse-up after focus changes");

    const int keyUpsBeforeAltRelease = secondPtr->keyUps;
    SDL_Event altTab{};
    altTab.type = SDL_KEYDOWN;
    altTab.key.keysym.sym = SDLK_TAB;
    altTab.key.keysym.mod = KMOD_ALT;
    const int secondKeyDownsBeforeAltTab = secondPtr->keyDowns;
    wm.handleEvent(altTab);
    check(secondPtr->keyDowns == secondKeyDownsBeforeAltTab,
          "shell-owned Alt+Tab keydown does not reach the focused client");
    SDL_Event tabRelease{};
    tabRelease.type = SDL_KEYUP;
    tabRelease.key.keysym.sym = SDLK_TAB;
    tabRelease.key.keysym.mod = KMOD_ALT;
    wm.handleEvent(tabRelease);
    check(secondPtr->keyUps == keyUpsBeforeAltRelease,
          "shell-owned Tab release does not reach the focused client");

    SDL_Event altRelease{};
    altRelease.type = SDL_KEYUP;
    altRelease.key.keysym.sym = SDLK_LALT;
    wm.handleEvent(altRelease);
    check(secondPtr->keyUps == keyUpsBeforeAltRelease,
          "shell-owned Alt release does not reach the focused client");

    SDL_Event ctrlEscape{};
    ctrlEscape.type = SDL_KEYDOWN;
    ctrlEscape.key.keysym.sym = SDLK_ESCAPE;
    ctrlEscape.key.keysym.mod = KMOD_CTRL;
    const int firstKeyDownsBeforeCtrlEscape = firstPtr->keyDowns;
    wm.handleEvent(ctrlEscape);
    check(firstPtr->keyDowns == firstKeyDownsBeforeCtrlEscape,
          "shell-owned Ctrl+Escape keydown does not reach the focused client");
    SDL_Event escapeRelease{};
    escapeRelease.type = SDL_KEYUP;
    escapeRelease.key.keysym.sym = SDLK_ESCAPE;
    escapeRelease.key.keysym.mod = KMOD_CTRL;
    wm.handleEvent(escapeRelease);
    check(secondPtr->keyUps == keyUpsBeforeAltRelease,
          "shell-owned Escape release does not reach the focused client");

    const int keyUpsBeforeMenuEscape = secondPtr->keyUps;
    check(wm.m_showStartMenu,
          "Ctrl+Escape leaves the Start menu open for bare Escape dismissal");
    SDL_Event menuEscape = ctrlEscape;
    menuEscape.key.keysym.mod = KMOD_NONE;
    wm.handleEvent(menuEscape);
    SDL_Event menuEscapeRelease = escapeRelease;
    menuEscapeRelease.key.keysym.mod = KMOD_NONE;
    wm.handleEvent(menuEscapeRelease);
    check(!wm.m_showStartMenu && secondPtr->keyUps == keyUpsBeforeMenuEscape,
          "bare Escape dismissal consumes the matching key release");

    const int keyUpsBeforeMenuEnter = secondPtr->keyUps;
    wm.handleEvent(ctrlEscape);
    wm.handleEvent(escapeRelease);
    SDL_Event menuEnter{};
    menuEnter.type = SDL_KEYDOWN;
    menuEnter.key.keysym.sym = SDLK_RETURN;
    wm.handleEvent(menuEnter);
    SDL_Event menuEnterRelease = menuEnter;
    menuEnterRelease.type = SDL_KEYUP;
    wm.handleEvent(menuEnterRelease);
    check(!wm.m_showStartMenu && secondPtr->keyUps == keyUpsBeforeMenuEnter,
          "Start-menu Enter activation consumes the matching key release");

    SDL_DestroyWindow(hostWindow);
    SDL_Quit();

    if (failures == 0) {
        std::cout << "ALL WINDOW MOUSE CAPTURE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
