#include <Vst/EffectModel.hpp>
#include <Vst/Widgets.hpp>
#include <Vst/Window.hpp>
#include <Vst/vst-compat.hpp>

namespace vst
{
/*
 *
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
  ManualWindow()
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
  */
void Window::setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setMinimumHeight(height);
  container->setMaximumHeight(height);
  container->setMinimumWidth(width);
  container->setMaximumWidth(width);
  container->setBaseSize({width, height});
}

Window::Window(const Model& e, const score::DocumentContext& ctx)
    : m_model{e}
{
  if(!e.fx)
    throw std::runtime_error("Cannot create UI");

  effect = e.fx;
  AEffect& fx = *e.fx->fx;

  // Some effects need a first getRect here to initialize stuff
  {
    ERect* vstRect{};
    fx.dispatcher(&fx, effEditGetRect, 0, 0, &vstRect, 0.f);
  }

  fx.dispatcher(&fx, effEditOpen, 0, 0, (void*)winId(), 0);

  // And a second getRect to get the actual coordinates once open
  auto rect = getRect(fx);
  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;
  setup_rect(this, width, height);
}
}
