#pragma once
#include <Dataflow/PortItem.hpp>
#include <Vst3/EffectModel.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <verdigris>

namespace Process
{
struct Context;
}
namespace vst3
{

class ControlInlet final : public Process::ControlInlet
{
  W_OBJECT(ControlInlet)
  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL(ControlInlet)
  ControlInlet(Id<Process::Port> c, QObject* parent)
      : Process::ControlInlet{"Control", std::move(c), parent}
  {
    setDomain(ossia::make_domain(0.f, 1.f));
    setInit(0.5);
  }

  ControlInlet(DataStream::Deserializer& vis, QObject* parent)
      : Process::ControlInlet{vis, parent, true}
  {
    vis.writeTo(*this);
    setDomain(ossia::make_domain(0.f, 1.f));
    setInit(0.5);
  }
  ControlInlet(JSONObject::Deserializer& vis, QObject* parent)
      : Process::ControlInlet{vis, parent, true}
  {
    vis.writeTo(*this);
    setDomain(ossia::make_domain(0.f, 1.f));
    setInit(0.5);
  }
  ControlInlet(DataStream::Deserializer&& vis, QObject* parent)
      : Process::ControlInlet{vis, parent, true}
  {
    vis.writeTo(*this);
    setDomain(ossia::make_domain(0.f, 1.f));
    setInit(0.5);
  }
  ControlInlet(JSONObject::Deserializer&& vis, QObject* parent)
      : Process::ControlInlet{vis, parent, true}
  {
    vis.writeTo(*this);
    setDomain(ossia::make_domain(0.f, 1.f));
    setInit(0.5);
  }

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }

  Steinberg::Vst::ParamID fxNum{};
  float value() const { return ossia::convert<float>(Process::ControlInlet::value()); }
  void setValue(float v) { Process::ControlInlet::setValue(v); }
};

struct VSTControlPortItem final : public Dataflow::AutomatablePortItem
{
public:
  using Dataflow::AutomatablePortItem::AutomatablePortItem;

  void setupMenu(QMenu& menu, const score::DocumentContext& ctx) override;
  bool on_createAutomation(
      const Scenario::IntervalModel& cst, std::function<void(score::Command*)> macro,
      const score::DocumentContext& ctx) override;
};

class VSTControlPortFactory final : public Process::PortFactory
{
public:
  ~VSTControlPortFactory() override;

  UuidKey<Process::Port> concreteKey() const noexcept override;

  Process::Port* load(const VisitorVariant& vis, QObject* parent) override;

  Dataflow::PortItem* makePortItem(
      Process::Inlet& port, const Process::Context& ctx, QGraphicsItem* parent,
      QObject* context) override;

  Dataflow::PortItem* makePortItem(
      Process::Outlet& port, const Process::Context& ctx, QGraphicsItem* parent,
      QObject* context) override;

  QGraphicsItem* makeControlItem(
      Process::ControlInlet& port, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context) override;

  QGraphicsItem* makeControlItem(
      Process::ControlOutlet& port, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context) override;

  void setupInletInspector(
      const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
      Inspector::Layout& lay, QObject* context) override;
};
}
