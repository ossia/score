#pragma once
// The cursors CursorTheme shows where no QCursor reaches the system: the
// frames, title bars and modally blocked windows on Windows
// (CursorThemeWin.cpp), the popup windows on macOS (CursorThemeMac.mm), which
// AppKit never asks for a cursor as they never become key.
class QWindow;

namespace score
{
class CursorTheme;
namespace cursor_native
{
#if defined(_WIN32) || defined(__APPLE__)
//! Hooks the system in, once per application.
void install(CursorTheme& theme);
void uninstall();
//! Frees the system cursors made from the theme's pixmaps, which are about
//! to be replaced.
void release();
#else
inline void install(CursorTheme&) { }
inline void uninstall() { }
inline void release() { }
#endif

#if defined(__APPLE__)
//! A popup window under the pointer: shows its cursor through AppKit.
bool showsCursorItself(const QWindow& w) noexcept;
void showCursorOf(const QWindow& w);
#endif
}
}
