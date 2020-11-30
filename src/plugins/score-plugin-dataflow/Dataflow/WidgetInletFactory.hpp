#pragma once
#include <Dataflow/PortItem.hpp>
#include <Process/Dataflow/PortListWidget.hpp>

namespace Dataflow
{
template <typename T>
struct WidgetInletFactory : public AutomatablePortFactory
{
  using Model_T = T;
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }

  void setupInletInspector(
      const Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override
  {
    using factory = typename Model_T::control_type;
    auto& ctrl = static_cast<const Model_T&>(port);
    auto widg = factory::make_widget(ctrl, ctrl, ctx, parent, parent);
    Process::PortWidgetSetup::setupControl(ctrl, widg, ctx, lay, parent);
  }

  QGraphicsItem* makeControlItem(
      Process::ControlInlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context) override
  {
    auto& ctrl = static_cast<Model_T&>(port);
    using widg_t = typename Model_T::control_type;
    return widg_t::make_item(ctrl, ctrl, ctx, nullptr, context);
  }
};

template <typename T>
struct WidgetOutletFactory : public Process::PortFactory
{
  using Model_T = T;
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }

  void setupOutletInspector(
      const Process::Outlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override
  {
    using factory = typename Model_T::control_type;
    auto& ctrl = static_cast<const Model_T&>(port);
    auto widg = factory::make_widget(ctrl, ctrl, ctx, parent, parent);
    Process::PortWidgetSetup::setupControl(ctrl, widg, ctx, lay, parent);
  }

  QGraphicsItem* makeControlItem(
      Process::ControlOutlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context) override
  {
    auto& ctrl = static_cast<Model_T&>(port);
    using widg_t = typename Model_T::control_type;
    return widg_t::make_item(ctrl, ctrl, ctx, nullptr, context);
  }
};
}
