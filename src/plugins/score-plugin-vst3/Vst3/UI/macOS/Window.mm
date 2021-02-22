#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>

#include <Vst3/UI/Window.hpp>
#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>


namespace vst3
{

static auto setSize(Steinberg::IPlugView& view, const Steinberg::ViewRect& r, Window& parentWindow, WindowContainer& wc)
{
  int w = r.getWidth();
  int h = r.getHeight();

  if(w < 5) w = 640;
  if(h < 5) h = 480;

  if(view.canResize() == Steinberg::kResultTrue)
  {
    parentWindow.resize(QSize{w, h});
  }
  else
  {
    parentWindow.setFixedSize(QSize{w, h});
  }
  if(wc.qwindow)
  {
    wc.qwindow->resize(w, h);
  }
  if(wc.container)
  {
    wc.container->move(0, 0);
    wc.container->setFixedSize(w, h);
  }

  return std::make_pair(w, h);
}
WindowContainer createVstWindowContainer(
    Window& parentWindow,
    const Model& e,
    const score::DocumentContext& ctx)
{
  WindowContainer wc;

  Steinberg::IPlugView& view = *e.fx.view;

  Steinberg::ViewRect r;

  view.getSize(&r);
  auto [w,h] = setSize(view, r, parentWindow, wc);

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(0, 0, w, h)];

  wc.qwindow = QWindow::fromWinId(reinterpret_cast<WId>(superview));
  wc.container = QWidget::createWindowContainer(wc.qwindow, &parentWindow);
  //view.setFrame(new PlugFrame{*wc.qwindow});
  view.attached((void*)superview, currentPlatform());

  wc.qwindow->show();

  wc.container->setObjectName("VSTWindow");


  view.getSize(&r);
  std::tie(w, h) = setSize(view, r, parentWindow, wc);

  NSRect frame = NSMakeRect(0, 0, w, h);
  [superview setFrame:frame];

  NSArray* subviews{};
  subviews = [superview subviews];
  id m_view = [[subviews objectAtIndex:0] retain];

  [[NSNotificationCenter defaultCenter]
      addObserverForName:@"NSViewFrameDidChangeNotification"
      object:m_view
      queue:nullptr
      usingBlock:^(NSNotification* notification) {
      Q_UNUSED(notification);

    qDebug() << "adjust editor size to" << parentWindow.sizeHint();
    // need to adjust the superview frame to be the same as the view frame
    /*
    [superview setFrame:[m_view frame]];
    wc.qwindow->setFixedSize(vst3::sizeHint(m_view));
    // adjust the size of the window to fit.
    // FIXME: this is indeed a bit dodgy ;)
    QApplication::processEvents();
    adjustSize();
    */

  //qDebug() << "got a notification" << vst::sizeHint(m_view);
}];

  wc.container->setVisible(true);
  wc.container->update();
  //eff->fx->dispatcher(eff->fx, effEditTop, 0, 0, 0, 0);

  {
    //if (view.canResize() == Steinberg::kResultTrue)
    // For some reason in that case the plug-ins aren't centered...
    {
      view.onSize(&r);
    }
  }
  [superview release];
  [pool release];

  return wc;
}

}
