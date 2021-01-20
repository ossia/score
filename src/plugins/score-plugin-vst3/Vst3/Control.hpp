#pragma once
#include <Dataflow/PortItem.hpp>
#include <Vst3/EffectModel.hpp>

#include <verdigris>

namespace Process
{
struct Context;
}
namespace vst3
{

class ControlInlet final : public Process::Inlet
{
  W_OBJECT(ControlInlet)
  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL(ControlInlet)
  ControlInlet(Id<Process::Port> c, QObject* parent) : Inlet{std::move(c), parent} { }

  ControlInlet(DataStream::Deserializer& vis, QObject* parent) : Inlet{vis, parent}
  {
    vis.writeTo(*this);
  }
  ControlInlet(JSONObject::Deserializer& vis, QObject* parent) : Inlet{vis, parent}
  {
    vis.writeTo(*this);
  }
  ControlInlet(DataStream::Deserializer&& vis, QObject* parent) : Inlet{vis, parent}
  {
    vis.writeTo(*this);
  }
  ControlInlet(JSONObject::Deserializer&& vis, QObject* parent) : Inlet{vis, parent}
  {
    vis.writeTo(*this);
  }

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }

  Steinberg::Vst::ParamID fxNum{};

  float value() const { return m_value; }
  void setValue(float v)
  {
    if (v != m_value)
    {
      m_value = v;
      valueChanged(v);
    }
  }

public:
  void valueChanged(float arg_1) W_SIGNAL(valueChanged, arg_1);

private:
  float m_value{};
};

struct VSTControlPortItem final : public Dataflow::AutomatablePortItem
{
public:
  using Dataflow::AutomatablePortItem::AutomatablePortItem;

  void setupMenu(QMenu& menu, const score::DocumentContext& ctx) override;
  bool on_createAutomation(
      Scenario::IntervalModel& cst,
      std::function<void(score::Command*)> macro,
      const score::DocumentContext& ctx) override;
};

class VSTControlPortFactory final : public Process::PortFactory
{
public:
  ~VSTControlPortFactory() override;

  UuidKey<Process::Port> concreteKey() const noexcept override;

  Process::Port* load(const VisitorVariant& vis, QObject* parent) override;

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

  void setupInletInspector(
      const Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override;
};
}
