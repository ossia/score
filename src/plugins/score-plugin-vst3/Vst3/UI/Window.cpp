#include <Vst3/UI/Window.hpp>
#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>

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

  wc.qwindow = new QWindow;
  wc.qwindow->resize(w, h);

  wc.container = QWidget::createWindowContainer(wc.qwindow, &parentWindow);
  wc.container->setGeometry(0, 0, w, h);

  view.setFrame(new PlugFrame{*wc.qwindow});
  view.attached((void*)wc.qwindow->winId(), currentPlatform());

  return wc;
}

}
