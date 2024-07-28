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
    return WidgetFactory::Button::make_widget(inlet, ctx, parent, parent);
  }
  QWidget* operator()(bool b) const noexcept
  {
    return WidgetFactory::Toggle::make_widget(inlet, ctx, parent, parent);
  }
  QWidget* operator()(int x) const noexcept
  {
    return WidgetFactory::IntSlider::make_widget(inlet, ctx, parent, parent);
  }
  QWidget* operator()(float x) const noexcept
  {
    return WidgetFactory::FloatSlider::make_widget(inlet, ctx, parent, parent);
  }
  QWidget* operator()(const std::string& c) const noexcept
  {
    return WidgetFactory::LineEdit::make_widget(inlet, ctx, parent, parent);
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
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  using namespace Process;
  auto& ctrl = static_cast<const Process::ControlInlet&>(port);
  auto& cctrl = const_cast<Process::ControlInlet&>(ctrl);

  if(auto widg = ctrl.value().apply(control_visitor{cctrl, ctx, parent}))
  {
    PortWidgetSetup::setupControl(ctrl, widg, ctx, lay, parent);
  }
  else
  {
    PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
  }
}

}
