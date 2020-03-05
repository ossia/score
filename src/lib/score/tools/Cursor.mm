#include "Cursor.hpp"

#include <AppKit/NSCursor.h>
#include <QApplication>
namespace score
{
static int hiddenCount = 0;
void hideCursor(bool hasCursor)
{
  [NSCursor hide];

  hiddenCount++;
}

void showCursor()
{
  while(hiddenCount >= 0) {
    [NSCursor unhide];
    hiddenCount--;
  }
  QApplication::restoreOverrideCursor();
}
}
