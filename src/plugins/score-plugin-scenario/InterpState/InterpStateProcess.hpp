#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/ProcessMetadata.hpp>
#include <State/Message.hpp>

#include <score_plugin_scenario_export.h>
namespace InterpState
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    InterpState::ProcessModel,
    "09fa6f72-55d5-4fee-8bc7-6f983c2e62d8",
    "InterpState",
    "State interpolation",
    Process::ProcessCategory::Automation,
    "Automations",
    "Interpolate between two states",
    "ossia score",
    (QStringList{"Curve", "Automation"}),
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
namespace InterpState
{
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(InterpState::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  ProcessModel(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~ProcessModel() override;

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent) : CurveProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  QString prettyName() const noexcept override;
  QString prettyValue(double x, double y) const noexcept override;

  State::MessageList startMessages() const;
  State::MessageList endMessages() const;

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;
};
}
