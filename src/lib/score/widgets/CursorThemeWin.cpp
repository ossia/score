// The cursors of the parts of score's windows Qt does not give a cursor to on
// Windows. Qt answers WM_SETCURSOR only while an override cursor is set
// (qwindowscontext.cpp, QtWindows::CursorEvent): otherwise DefWindowProc
// shows the system's arrow on the title bar and over a window blocked by a
// modal one (Qt disables it, QWindowsWindow::windowEvent), and the system's
// sizing arrows on the frame. score shows its own there, rendered at the
// DPI of the window's monitor from the same images as its QCursors.
//
// The native dialogs (the file dialogs, class #32770) have their own window
// procedure: their messages never reach Qt's, and they keep the system's
// cursors.
#if defined(_WIN32)
#include "CursorTheme.hpp"
#include "CursorThemeNative.hpp"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QScreen>
#include <QtGui/qscreen_platform.h>

#include <algorithm>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace score::cursor_native
{
namespace
{
static_assert(win32_hit::Error == HTERROR);
static_assert(win32_hit::Transparent == HTTRANSPARENT);
static_assert(win32_hit::Nowhere == HTNOWHERE);
static_assert(win32_hit::Client == HTCLIENT);
static_assert(win32_hit::Caption == HTCAPTION);
static_assert(win32_hit::SysMenu == HTSYSMENU);
static_assert(win32_hit::GrowBox == HTGROWBOX);
static_assert(win32_hit::Menu == HTMENU);
static_assert(win32_hit::HScroll == HTHSCROLL);
static_assert(win32_hit::VScroll == HTVSCROLL);
static_assert(win32_hit::MinButton == HTMINBUTTON);
static_assert(win32_hit::MaxButton == HTMAXBUTTON);
static_assert(win32_hit::Left == HTLEFT);
static_assert(win32_hit::Right == HTRIGHT);
static_assert(win32_hit::Top == HTTOP);
static_assert(win32_hit::TopLeft == HTTOPLEFT);
static_assert(win32_hit::TopRight == HTTOPRIGHT);
static_assert(win32_hit::Bottom == HTBOTTOM);
static_assert(win32_hit::BottomLeft == HTBOTTOMLEFT);
static_assert(win32_hit::BottomRight == HTBOTTOMRIGHT);
static_assert(win32_hit::Border == HTBORDER);
static_assert(win32_hit::Object == HTOBJECT);
static_assert(win32_hit::Close == HTCLOSE);
static_assert(win32_hit::Help == HTHELP);

struct NativeCursor
{
  qint64 key{};
  HCURSOR handle{};
};

//! Made from the theme's pixmaps, by their cache key: a few per screen scale.
std::vector<NativeCursor> g_cursors;
//! Released while the system showed them: destroyed at the next release.
std::vector<HCURSOR> g_retired;

HCURSOR makeCursor(const QCursor& c)
{
  const QPixmap pm = c.pixmap();
  if(pm.isNull())
    return nullptr;

  // As qwindowscursor.cpp createPixmapCursor does for an unscaled pixmap: a
  // 32 bit bitmap with straight alpha, and an empty mask.
  const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
  HBITMAP color = img.toHBITMAP();
  const int maskStride = ((img.width() + 15) / 16) * 2;
  std::vector<unsigned char> maskBits(std::size_t(maskStride) * img.height(), 0);
  HBITMAP mask = CreateBitmap(img.width(), img.height(), 1, 1, maskBits.data());

  // The QCursor's hotspot is in device independent pixels, and the pixmap's
  // ratio is the monitor's factor: Qt multiplies the two the same way.
  const qreal dpr = pm.devicePixelRatio();
  ICONINFO ii{};
  ii.fIcon = FALSE;
  ii.xHotspot = DWORD(qRound(c.hotSpot().x() * dpr));
  ii.yHotspot = DWORD(qRound(c.hotSpot().y() * dpr));
  ii.hbmMask = mask;
  ii.hbmColor = color;
  HCURSOR cursor = nullptr;
  if(color && mask)
    cursor = CreateIconIndirect(&ii);
  if(color)
    DeleteObject(color);
  if(mask)
    DeleteObject(mask);
  return cursor;
}

HCURSOR cursorOf(const QCursor& c)
{
  if(c.shape() != Qt::BitmapCursor)
    return nullptr;
  const qint64 key = c.pixmap().cacheKey();
  for(const auto& n : g_cursors)
    if(n.key == key)
      return n.handle;
  HCURSOR h = makeCursor(c);
  if(h)
    g_cursors.push_back({key, h});
  return h;
}

//! The screen Qt puts a window on: the monitor with the most of it.
const QScreen* screenOf(HWND hwnd)
{
  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  for(QScreen* s : QGuiApplication::screens())
    if(auto* native = s->nativeInterface<QNativeInterface::QWindowsScreen>())
      if(native->handle() == monitor)
        return s;
  return QGuiApplication::primaryScreen();
}

class SetCursorFilter final : public QAbstractNativeEventFilter
{
public:
  explicit SetCursorFilter(CursorTheme& theme)
      : m_theme{theme}
  {
  }

  bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override
  {
    // Sent, not posted: it only comes through Qt's window procedure.
    if(type != "windows_generic_MSG")
      return false;
    auto* msg = static_cast<MSG*>(message);
    if(msg->message != WM_SETCURSOR)
      return false;

    // Qt shows the override cursor everywhere itself.
    if(QGuiApplication::overrideCursor())
      return false;

    // The window under the pointer is the one asked; a child window that
    // is not Qt's passes its WM_SETCURSOR up with its own handle.
    const HWND hwnd = msg->hwnd;
    if(reinterpret_cast<HWND>(msg->wParam) != hwnd)
      return false;

    const HWND root = GetAncestor(hwnd, GA_ROOT);
    const bool blocked = root && !IsWindowEnabled(root);
    const int hitTest = short(LOWORD(msg->lParam));
    const CursorArtwork artwork = windowsHitTestCursor(hitTest, blocked);
    if(artwork == CursorArtwork::Count)
      return false;

    const double scale = m_theme.screenScaleOf(screenOf(hwnd));
    const HCURSOR cursor = cursorOf(m_theme.cursor(artwork, scale));
    if(!cursor)
      return false;

    // A click on a blocked window beeps and flashes the modal one:
    // DefWindowProc does that, and picks the system's arrow, which is
    // replaced right after.
    const UINT mouseMessage = HIWORD(msg->lParam);
    if(mouseMessage != 0 && mouseMessage != WM_MOUSEMOVE)
      DefWindowProcW(hwnd, WM_SETCURSOR, msg->wParam, msg->lParam);

    SetCursor(cursor);
    *result = TRUE;
    return true;
  }

private:
  CursorTheme& m_theme;
};

SetCursorFilter* g_filter{};

void releaseAtExit()
{
  release();
  for(HCURSOR h : g_retired)
    DestroyCursor(h);
  g_retired.clear();
}
}

void install(CursorTheme& theme)
{
  if(g_filter || !qGuiApp)
    return;
  g_filter = new SetCursorFilter{theme};
  qGuiApp->installNativeEventFilter(g_filter);
  qAddPostRoutine(releaseAtExit);
}

void uninstall()
{
  if(!g_filter)
    return;
  if(qGuiApp)
    qGuiApp->removeNativeEventFilter(g_filter);
  delete g_filter;
  g_filter = nullptr;
}

void release()
{
  for(HCURSOR h : g_retired)
    if(h != GetCursor())
      DestroyCursor(h);
  std::erase_if(g_retired, [](HCURSOR h) { return h != GetCursor(); });

  const HCURSOR shown = GetCursor();
  for(const auto& n : g_cursors)
  {
    if(n.handle == shown)
      g_retired.push_back(n.handle);
    else
      DestroyCursor(n.handle);
  }
  g_cursors.clear();
}
}
#endif
