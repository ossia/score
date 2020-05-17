#pragma once
#include <Automation/AutomationProcessMetadata.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <State/Address.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>

#include <Interpolation/InterpolationProcess.hpp>

#include <utility>
#include <vector>

struct DataStreamInput;
struct DataStreamOutput;

namespace Process
{
class ProcessModel;
}
template <typename Object>
class Path;

namespace Scenario
{
class IntervalModel;
namespace Command
{

// MOVEME
class SCORE_PLUGIN_SCENARIO_EXPORT CreateProcessAndLayers : public score::Command
{
public:
  CreateProcessAndLayers() = default;

  CreateProcessAndLayers(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> procId,
      UuidKey<Process::ProcessModel> key);

  void undo(const score::DocumentContext& ctx) const final override;
  const Id<Process::ProcessModel>& processId() const { return m_addProcessCmd.processId(); }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  AddOnlyProcessToInterval m_addProcessCmd;
  std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;
};

class SCORE_PLUGIN_SCENARIO_EXPORT CreateAutomationFromStates final : public CreateProcessAndLayers
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      CreateAutomationFromStates,
      "CreateAutomationFromStates")
public:
  CreateAutomationFromStates(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> curveId,
      State::AddressAccessor address,
      const Curve::CurveDomain&,
      bool tween = false);

  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;

  Curve::CurveDomain m_dom{};
  bool m_tween{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT CreateGradient final : public CreateProcessAndLayers
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateGradient, "CreateGradientFromStates")
public:
  CreateGradient(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> curveId,
      State::AddressAccessor address,
      QColor start,
      QColor end,
      bool tween = false);

  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;
  QColor m_start{}, m_end{};
  bool m_tween{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT CreateInterpolationFromStates final
    : public CreateProcessAndLayers
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      CreateInterpolationFromStates,
      "CreateInterpolationFromStates")
public:
  CreateInterpolationFromStates(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> curveId,
      State::AddressAccessor address,
      ossia::value start,
      ossia::value end,
      bool tween = false);

  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;
  ossia::value m_start{}, m_end{};
  bool m_tween{};
};
}
}
