// The cursor of Qt's popup windows on macOS: menus, the lists of combo
// boxes, tool tips. Qt makes them NSPanels that refuse to become key
// (qnswindow.mm canBecomeKeyWindow), and a QNSView only shows its cursor in
// cursorUpdate:, which AppKit sends to the key window, or when
// QCocoaWindow::setWindowCursor synthesizes one, which it does for the key
// window and utility panels only. Over a popup AppKit keeps its arrow.
// score sets the popup's cursor itself, from the pixmap of its QCursor, at
// every move of the pointer over it.
#include "CursorThemeNative.hpp"

#include <QCursor>
#include <QImage>
#include <QPixmap>
#include <QWindow>

#include <vector>

#import <AppKit/AppKit.h>

namespace score::cursor_native
{
namespace
{
struct NativeCursor
{
  qint64 key{};
  CFTypeRef cursor{};
};

//! Made from the theme's pixmaps, by their cache key.
std::vector<NativeCursor> g_cursors;

NSCursor* makeCursor(const QCursor& c)
{
  const QPixmap pm = c.pixmap();
  if(pm.isNull())
    return nil;

  // As qcocoacursor.mm createCursorFromPixmap: an image of the pixmap's size
  // over its ratio in points, the hotspot in points.
  CGImageRef cg = pm.toImage().toCGImage();
  if(!cg)
    return nil;
  const qreal dpr = pm.devicePixelRatio();
  NSImage* image = [[NSImage alloc]
      initWithCGImage:cg
                 size:NSMakeSize(pm.width() / dpr, pm.height() / dpr)];
  CGImageRelease(cg);
  NSCursor* cursor = [[NSCursor alloc] initWithImage:image
                                             hotSpot:NSMakePoint(c.hotSpot().x(), c.hotSpot().y())];
#if !__has_feature(objc_arc)
  [image release];
  [cursor autorelease];
#endif
  return cursor;
}
}

void install(CursorTheme&) { }

void uninstall() { }

void release()
{
  for(const auto& n : g_cursors)
    CFRelease(n.cursor);
  g_cursors.clear();
}

bool showsCursorItself(const QWindow& w) noexcept
{
  // The windows Qt makes panels that never become key.
  const auto type = w.type();
  return (type & Qt::Popup) == Qt::Popup && type != Qt::Tool;
}

void showCursorOf(const QWindow& w)
{
  if(!NSApp.active)
    return;
  const QCursor c = w.cursor();
  if(c.shape() != Qt::BitmapCursor)
    return;

  const qint64 key = c.pixmap().cacheKey();
  NSCursor* cursor = nil;
  for(const auto& n : g_cursors)
    if(n.key == key)
      cursor = (__bridge NSCursor*)n.cursor;
  if(!cursor)
  {
    @autoreleasepool
    {
      NSCursor* made = makeCursor(c);
      if(!made)
        return;
      g_cursors.push_back({key, CFBridgingRetain(made)});
      cursor = made;
      [cursor set];
    }
    return;
  }
  [cursor set];
}
}
