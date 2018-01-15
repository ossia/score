#include <AppKit/AppKit.h>
//#include <Cocoa/Cocoa.h>
#include <Media/Effect/VST/vst-compat.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>
#include <iostream>
#include <QMacCocoaViewContainer>
#include <QDialog>
#include "vstwindow.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QDebug>
#include <QMainWindow>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <QTimer>

namespace Media::VST
{
QSize sizeHint(NSView* m_view) {

  QSize ret;

  if (m_view == 0) {
    ret = QSize(0, 0);
  }

  NSRect frame = [m_view frame];
  ret = QSize(frame.size.width, frame.size.height);

  return ret;

}


VSTWindow::VSTWindow(AEffect& eff, ERect rect): effect{eff}
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  qDebug() << rect.top << rect.left;

  setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
  setWindowFlag(Qt::WindowMinimizeButtonHint, false);
  setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(rect.top, rect.left, rect.right, rect.bottom)];

  effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)superview, 0);

  ERect* vstRect{};

  effect.dispatcher(&effect, effEditGetRect, 0, 0, &vstRect, 0.0f);
  if(vstRect)
  {
    rect = *vstRect;
  }

  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;

  auto container = new QMacCocoaViewContainer{superview, this};

  NSRect frame = NSMakeRect(rect.left, rect.top,
                            rect.right, rect.bottom);

  [superview setFrame:frame];
  container->resize(width, height);

  NSArray* subviews;
  subviews = [superview subviews];
  auto m_view = [[subviews objectAtIndex:0] retain];

  [[NSNotificationCenter defaultCenter] addObserverForName:@"NSViewFrameDidChangeNotification" object:m_view queue:nil usingBlock:^(NSNotification* notification) {
      Q_UNUSED(notification);

      //qDebug() << "adjust editor size to" << sizeHint();
      // need to adjust the superview frame to be the same as the view frame
      [superview setFrame:[m_view frame]];
      setFixedSize(Media::VST::sizeHint(m_view));
      // adjust the size of the window to fit.
      // FIXME: this is indeed a bit dodgy ;)
      QApplication::processEvents();
      adjustSize();

    qDebug() << "got a notification" << Media::VST::sizeHint(m_view);
  }];

  //[superview setFrame:NSMakeRect(0, 0, width, height)];

  qDebug() << Media::VST::sizeHint(m_view);
  setFixedSize(QSize(width, height));

  show();
  container->setVisible(true);
  container->update();
  effect.dispatcher(&effect, effEditTop, 0, 0, 0, 0);

  reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = reinterpret_cast<intptr_t>(container);
  [superview release];
  [pool release];
}

VSTWindow::~VSTWindow()
{
  effect.dispatcher(&effect, effEditClose, 0, 0, nullptr, 0);
  if(effect.resvd1)
  {
    reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = 0;
  }
}
}
