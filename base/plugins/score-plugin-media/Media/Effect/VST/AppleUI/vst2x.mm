#include <AppKit/AppKit.h>
//#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>
#include <iostream>
#include <QMacCocoaViewContainer>
#include <aeffect.h>
#include "vstwindow.h"
// TODO have a look at https://github.com/alex-weej/Evilnote/blob/master/vsteditorwidget.mm
void show_vst2_editor(AEffect* effect, uint16_t width, uint16_t height)
{
  auto container = reinterpret_cast<QMacCocoaViewContainer*>(effect->resvd2);
  if(container)
  {
    effect->dispatcher(effect, effEditOpen, 0, 0, (void*)container->cocoaView(), 0);
    container->show();
    return;
  }

  NSView *view = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, height)];
  container = new QMacCocoaViewContainer{view};
  container->resize(width, height);
  container->window()->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
  container->window()->setWindowFlag(Qt::WindowMinimizeButtonHint, false);
  container->window()->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  container->window()->setFixedSize(width, height);

  NSRect frame = NSMakeRect(0, 0, 0, 0);
  frame.origin.x = 0;
  frame.origin.y = 0;
  frame.size.width = width;
  frame.size.height = height;

  [view setFrame:frame];

  effect->dispatcher(effect, effEditOpen, 0, 0, (void*)view, 0);
  container->show();

  effect->resvd2 = reinterpret_cast<VstIntPtr>(container);
}

void hide_vst2_editor(AEffect* effect)
{
  effect->dispatcher(effect, effEditClose, 0, 0, nullptr, 0);
  auto window = reinterpret_cast<QMacCocoaViewContainer*>(effect->resvd2);
  window->close();
  window->deleteLater();
  effect->resvd2 = 0;
}
