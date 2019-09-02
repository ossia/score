#pragma once
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <score_plugin_scenario_export.h>
namespace Scenario
{
class IntervalModel;
}
namespace Dataflow
{
class CableItem;

class SCORE_PLUGIN_SCENARIO_EXPORT AutomatablePortItem : public PortItem
{
public:
  using PortItem::PortItem;
  ~AutomatablePortItem() override;

  void setupMenu(QMenu&, const score::DocumentContext& ctx) override;
  void on_createAutomation(const score::DocumentContext& m_context);
  virtual bool on_createAutomation(
      Scenario::IntervalModel& parent,
      std::function<void(score::Command*)> macro,
      const score::DocumentContext& m_context);

  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
};

class AutomatablePortFactory : public Process::PortFactory
{
public:
  ~AutomatablePortFactory() override = default;

private:
  Dataflow::PortItem* makeItem(
      Process::Inlet& port,
      const Process::Context& ctx,
      QGraphicsItem* parent,
      QObject* context) override;

  Dataflow::PortItem* makeItem(
      Process::Outlet& port,
      const Process::Context& ctx,
      QGraphicsItem* parent,
      QObject* context) override;
};

template <typename Model_T>
class AutomatablePortFactory_T final : public AutomatablePortFactory
{
public:
  ~AutomatablePortFactory_T() override = default;

private:
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
};

using InletFactory = AutomatablePortFactory_T<Process::Inlet>;
using OutletFactory = AutomatablePortFactory_T<Process::Outlet>;
using ControlOutletFactory = AutomatablePortFactory_T<Process::ControlOutlet>;

struct ControlInletFactory final : public AutomatablePortFactory
{
  using Model_T = Process::ControlInlet;
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
      Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override;
};

class PortTooltip final : public QWidget
{
public:
  PortTooltip(
      const score::DocumentContext& ctx,
      const Process::Port& p,
      QWidget* parent);
};

class PortInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("1e7166bb-278a-49ce-b6a9-d662b8cd8dd2")
public:
  PortInspectorFactory() : InspectorWidgetFactory{} {}

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new PortTooltip{
        doc, safe_cast<const Process::Port&>(*sourceElements.first()), parent};
  }

  bool matches(const InspectedObjects& objects) const override
  {
    return dynamic_cast<const Process::Port*>(objects.first());
  }
};
}

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
      Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override
  {
    using factory = typename Model_T::control_type;
    auto& ctrl = static_cast<Model_T&>(port);
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
}
