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
WindowContainer createVstWindowContainer(
    Window& parentWindow,
    const Model& e,
    const score::DocumentContext& ctx)
{
  WindowContainer wc;

  Steinberg::IPlugView& view = *e.fx.view;

  Steinberg::ViewRect r;

  view.getSize(&r);
  auto [w,h] = wc.setSizeFromQt(view, r, parentWindow);

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(0, 0, w, h)];

  wc.qwindow = QWindow::fromWinId(reinterpret_cast<WId>(superview));
  wc.container = QWidget::createWindowContainer(wc.qwindow, &parentWindow);
  view.setFrame(new PlugFrame{parentWindow, wc});
  view.attached((void*)superview, currentPlatform());

  wc.qwindow->show();

  wc.container->setObjectName("VSTWindow");


  view.getSize(&r);
  std::tie(w, h) = wc.setSizeFromQt(view, r, parentWindow);

  NSRect frame = NSMakeRect(0, 0, w, h);
  [superview setFrame:frame];

  NSArray* subviews{};
  subviews = [superview subviews];
  id m_view = [[subviews objectAtIndex:0] retain];

  auto adjustSize= [v=&view] (double w, double h){
    Steinberg::ViewRect r;
    r.left = 0;
    r.top = 0;
    r.right = w;
    r.bottom = h;
    v->onSize(&r);
  };
  [[NSNotificationCenter defaultCenter]
      addObserverForName:@"NSViewFrameDidChangeNotification"
      object:m_view
      queue:nullptr
      usingBlock:^(NSNotification* notification) {
      Q_UNUSED(notification);

    auto sz = [ m_view frame ];
    qDebug() << "adjust editor size to" << parentWindow.geometry() <<  sz.size.width << sz.size.height;

    // need to adjust the superview frame to be the same as the view frame

    adjustSize(parentWindow.geometry().width(), parentWindow.geometry().height());

    [superview setFrame:[m_view frame]];

    /*wc.qwindow->setFixedSize(vst3::sizeHint(m_view));
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
      qDebug() << r.getWidth() << r.getHeight();
      view.onSize(&r);
    }
  }
  [superview release];
  [pool release];

  return wc;
}

}
