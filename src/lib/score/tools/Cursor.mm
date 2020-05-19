#include "Cursor.hpp"

#include <AppKit/NSCursor.h>
#include <QApplication>
#include <QDebug>
#import <ApplicationServices/ApplicationServices.h>
namespace score
{
static int hiddenCount = 0;
void hideCursor(bool hasCursor)
{
    // 10.9 and later
    const void * keys[] = { kAXTrustedCheckOptionPrompt };
    const void * values[] = { kCFBooleanTrue };

    CFDictionaryRef options = CFDictionaryCreate(
            kCFAllocatorDefault,
            keys,
            values,
            sizeof(keys) / sizeof(*keys),
            &kCFCopyStringDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

   if(! AXIsProcessTrustedWithOptions(options))
     qDebug("Warning: accessibility does not seem allowed for score which will make the minimap unuseable. \n"
            "Go check your mac preferences > Security > Confidentiality > Accessibility and check that score "
            "has its box checked. ");

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
