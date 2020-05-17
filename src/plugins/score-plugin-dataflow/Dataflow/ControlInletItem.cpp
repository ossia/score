#include "ControlInletItem.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/PortListWidget.hpp>

#include <ossia/network/domain/domain.hpp>

namespace Dataflow
{

template <typename T>
struct minmax
{
  const ossia::domain& domain;
  auto getMin() const { return domain.convert_min<T>(); }
  auto getMax() const { return domain.convert_max<T>(); }
};

struct control_visitor
{
  Process::ControlInlet& inlet;
  const score::DocumentContext& ctx;
  QWidget* parent{};
  QWidget* operator()(ossia::impulse) const noexcept
  {
    struct t
    {
    } b;
    return WidgetFactory::Button::make_widget(b, inlet, ctx, parent, parent);
  }
  QWidget* operator()(bool b) const noexcept
  {
    struct t
    {
    } tog;
    return WidgetFactory::Toggle::make_widget(tog, inlet, ctx, parent, parent);
  }
  QWidget* operator()(int x) const noexcept
  {
    minmax<int> sl{inlet.domain().get()};
    return WidgetFactory::IntSlider::make_widget(sl, inlet, ctx, parent, parent);
  }
  QWidget* operator()(float x) const noexcept
  {
    minmax<float> sl{inlet.domain().get()};
    return WidgetFactory::FloatSlider::make_widget(sl, inlet, ctx, parent, parent);
  }
  QWidget* operator()(const std::string& c) const noexcept
  {
    struct le
    {
    } l;
    return WidgetFactory::LineEdit::make_widget(l, inlet, ctx, parent, parent);
  }
  template <typename T>
  QWidget* operator()(const T&) const noexcept
  {
    SCORE_TODO;
    return nullptr;
  }
  QWidget* operator()() const noexcept
  {
    SCORE_TODO;
    return nullptr;
  }
};

void ControlInletFactory::setupInletInspector(
    Process::Inlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  using namespace Process;
  auto& ctrl = static_cast<Process::ControlInlet&>(port);
  auto widg = ctrl.value().apply(control_visitor{ctrl, ctx, parent});
  if (widg)
  {
    PortWidgetSetup::setupControl(ctrl, widg, ctx, lay, parent);
  }
  else
  {
    PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
  }
}

}
