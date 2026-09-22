#include "WindowManager.hpp"
#include "DesktopIcons.hpp"
#include "StartMenuFilter.hpp"
#include "WallpaperImage.hpp"
#include "SessionFormat.hpp"
#include "../app/App.hpp"
#include "../detail/AtomicFile.hpp"
#include "../detail/RendererClip.hpp"
#include "../app/FilePath.hpp"
#include "../app/TerminalApp.hpp"
#include "../app/TextEditorApp.hpp"
#include "../app/FilesystemApp.hpp"
#include "../app/SettingsApp.hpp"
#include "../app/DrawingApp.hpp"
#include "../app/SnakeApp.hpp"
#include "../app/MinesweeperApp.hpp"
#include "../app/PongApp.hpp"
#include "../app/BreakoutApp.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace monolith::window {

namespace {

using monolith::detail::RendererClipState;
using monolith::detail::captureRendererClip;
using monolith::detail::restoreRendererClip;
using monolith::detail::intersectRendererClip;
using monolith::detail::RendererBlendState;
using monolith::detail::captureRendererBlend;
using monolith::detail::restoreRendererBlend;
using monolith::detail::RendererDrawColorState;
using monolith::detail::captureRendererDrawColor;
using monolith::detail::restoreRendererDrawColor;

#include "detail/wm_body_01.inc"
#include "detail/wm_body_02.inc"
#include "detail/wm_body_03.inc"
#include "detail/wm_body_04.inc"
#include "detail/wm_body_05.inc"
#include "detail/wm_body_06.inc"
#include "detail/wm_body_07.inc"
#include "detail/wm_body_08.inc"
#include "detail/wm_body_08b.inc"
#include "detail/wm_desktop_icons.inc"
#include "detail/wm_body_09.inc"
