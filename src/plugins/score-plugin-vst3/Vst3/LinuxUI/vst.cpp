#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#include <Media/Effect/VST/vst-compat.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
namespace vst3
{
void VSTWindow::setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setMinimumHeight(height);
  container->setMaximumHeight(height);
  container->setMinimumWidth(width);
  container->setMaximumWidth(width);
  container->setBaseSize({width, height});
}

VSTWindow::VSTWindow(const VSTEffectModel& e, const score::DocumentContext& ctx) : m_model{e}
{
  if (!e.fx)
    throw std::runtime_error("Cannot create UI");
  effect = e.fx;
  AEffect& fx = *e.fx->fx;
  if (hasUI(fx))
  {
    auto rect = getRect(fx);
    auto width = rect.right - rect.left;
    auto height = rect.bottom - rect.top;

    fx.dispatcher(&fx, effEditOpen, 0, 0, (void*)winId(), 0);
    setup_rect(this, width, height);
  }
  else
  {
    auto widg = new QGraphicsScene{this};
    auto lay = new QHBoxLayout{this};
    auto v = new QGraphicsView{this};
    lay->addWidget(v);
    m_defaultWidg = v;

    for (auto& inlet : e.controls)
    {
      auto sl = VSTFloatSlider::make_item(&fx, *inlet.second, ctx, nullptr, this);
      widg->addItem(sl);
    }
  }
}
}
#endif
