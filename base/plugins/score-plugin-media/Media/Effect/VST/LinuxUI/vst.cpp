#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#include <Media/Effect/VST/vst-compat.hpp>
namespace Media::VST
{
static
auto setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setMinimumHeight(height);
  container->setMaximumHeight(height);
  container->setMinimumWidth(width);
  container->setMaximumWidth(width);
  container->setBaseSize({width, height});
}

VSTWindow::VSTWindow(AEffect& eff, ERect rect): effect{eff}
{
  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;

  effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)winId(), 0);
  setup_rect(this, width, height);
}

VSTWindow::~VSTWindow()
{
}
}
