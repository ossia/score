#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>

#include <Vst/vst-compat.hpp>
#include <Vst/Window.hpp>
#include <Vst/Widgets.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/AppleUI/vstwindow.h>

#include <QWindow>
#include <QApplication>
#include <QDebug>

#include <iostream>

namespace vst
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

  auto c = container->findChild<QWidget*>("VSTWindow");
  if(c)
  {
    c->setFixedSize(width, height);
  }
}

void Window::initNativeWindow(const Model& e, const score::DocumentContext& ctx)
{
  auto& eff = e.fx;
  auto rect = getRect(*e.fx->fx);
  effect = e.fx;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(rect.top, rect.left, rect.right, rect.bottom)];

  int res = eff->fx->dispatcher(eff->fx, effEditOpen, 0, 0, (void*)superview, 0);
  if(!res)
  {
    [pool release];
    throw std::runtime_error("Cannot open UI");
  }
  ERect* vstRect{};

  eff->fx->dispatcher(eff->fx, effEditGetRect, 0, 0, &vstRect, 0.0f);
  if(vstRect)
  {
    rect = *vstRect;
  }

  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;

  auto superview_window = QWindow::fromWinId(reinterpret_cast<WId>(superview));
  auto container = QWidget::createWindowContainer(superview_window, this);
  container->setObjectName("VSTWindow");

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
      setFixedSize(vst::sizeHint(m_view));
      // adjust the size of the window to fit.
      // FIXME: this is indeed a bit dodgy ;)
      QApplication::processEvents();
      adjustSize();
  }];

  //[superview setFrame:NSMakeRect(0, 0, width, height)];

  setFixedSize(QSize(width, height));

  show();
  container->setVisible(true);
  container->update();
  eff->fx->dispatcher(eff->fx, effEditTop, 0, 0, 0, 0);

  [superview release];
  [pool release];
}

}
