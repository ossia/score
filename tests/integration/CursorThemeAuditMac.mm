// Reads back the cursor AppKit shows, for CursorThemeAuditTest.
#include <QPointF>
#include <QSizeF>

#import <AppKit/AppKit.h>

namespace score_test
{
bool macApplicationActive()
{
  return NSApp.active;
}

bool macSystemCursor(QSizeF& points, QPointF& hotspot)
{
  @autoreleasepool
  {
    NSCursor* c = NSCursor.currentSystemCursor;
    if(!c || !c.image)
      return false;
    points = QSizeF(c.image.size.width, c.image.size.height);
    hotspot = QPointF(c.hotSpot.x, c.hotSpot.y);
    return true;
  }
}
}
