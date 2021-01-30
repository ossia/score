#include <AppKit/AppKit.h>
#include <Vst/vst-compat.hpp>
#include <Vst/Widgets.hpp>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>
#include <iostream>
#include <QMacCocoaViewContainer>
#include <Vst/AppleUI/vstwindow.h>
#include <QApplication>
#include <QWindow>
#include <Vst/EffectModel.hpp>
#include <QDebug>

namespace Vst
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

void Window::setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setFixedSize(width, height);
  qDebug()<< "setup_rect" << container;
  auto c = container->findChild<QWidget*>("VSTWindow");
  if(c)
  {
    c->setFixedSize(width, height);
  }
}

Window::Window(const VSTEffectModel& e, const score::DocumentContext& ctx)
  : m_model{e}
{
  if(!e.fx)
    throw std::runtime_error("Cannot create UI");
  auto& eff = e.fx;
  auto rect = getRect(*e.fx->fx);
  effect = e.fx;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];


  setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
  //setWindowFlag(Qt::WindowMinimizeButtonHint, false);
  //setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(rect.top, rect.left, rect.right, rect.bottom)];

  eff->fx->dispatcher(eff->fx, effEditOpen, 0, 0, (void*)superview, 0);

  ERect* vstRect{};

  eff->fx->dispatcher(eff->fx, effEditGetRect, 0, 0, &vstRect, 0.0f);
  if(vstRect)
  {
    rect = *vstRect;
  }

  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;
  qDebug() << rect.top << rect.left << rect.bottom << rect.right << width << height;

  auto superview_window = QWindow::fromWinId(reinterpret_cast<WId>(superview));
  auto container = QWidget::createWindowContainer(superview_window, this);
  container->setObjectName("VSTWindow");
  qDebug() << container;

  NSRect frame = NSMakeRect(rect.left, rect.top,
                            width, height);

  [superview setFrame:frame];
  container->resize(width, height);

  NSArray* subviews;
  subviews = [superview subviews];
  id m_view = [[subviews objectAtIndex:0] retain];

  [[NSNotificationCenter defaultCenter] addObserverForName:@"NSViewFrameDidChangeNotification" object:m_view queue:nullptr usingBlock:^(NSNotification* notification) {
      Q_UNUSED(notification);

      //qDebug() << "adjust editor size to" << sizeHint();
      // need to adjust the superview frame to be the same as the view frame
      [superview setFrame:[m_view frame]];
      setFixedSize(Vst::sizeHint(m_view));
      // adjust the size of the window to fit.
      // FIXME: this is indeed a bit dodgy ;)
      QApplication::processEvents();
      adjustSize();

    qDebug() << "got a notification" << Vst::sizeHint(m_view);
  }];

  //[superview setFrame:NSMakeRect(0, 0, width, height)];

  qDebug() << Vst::sizeHint(m_view);
  setFixedSize(QSize(width, height));

  show();
  container->setVisible(true);
  container->update();
  eff->fx->dispatcher(eff->fx, effEditTop, 0, 0, 0, 0);

  [superview release];
  [pool release];
}

}
