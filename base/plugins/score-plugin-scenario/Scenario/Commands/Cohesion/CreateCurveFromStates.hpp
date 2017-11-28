#pragma once
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Address.hpp>
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <utility>
#include <vector>

#include <Automation/AutomationProcessMetadata.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <score/application/ApplicationContext.hpp>
#include <Process/ProcessList.hpp>

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
template <typename ProcessModel_T>
class SCORE_PLUGIN_SCENARIO_EXPORT CreateProcessAndLayers
    : public score::Command
{
public:
  CreateProcessAndLayers() = default;
  CreateProcessAndLayers(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> procId)
      : m_addProcessCmd{std::move(interval), std::move(procId),
                        Metadata<ConcreteKey_k, ProcessModel_T>::get()}
  {
    m_slotsCmd.reserve(slotList.size());
    for (const auto& elt : slotList)
    {
      m_slotsCmd.emplace_back(elt, procId);
    }
  }

  void undo(const score::DocumentContext& ctx) const final override
  {
    for (const auto& cmd : m_slotsCmd)
      cmd.undo(ctx);
    m_addProcessCmd.undo(ctx);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_addProcessCmd.serialize();
    s << (int32_t)m_slotsCmd.size();
    for (const auto& elt : m_slotsCmd)
    {
      s << elt.serialize();
    }
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    QByteArray a;
    s >> a;
    m_addProcessCmd.deserialize(a);

    int32_t n = 0;
    s >> n;
    m_slotsCmd.resize(n);
    for (int i = 0; i < n; i++)
    {
      QByteArray b;
      s >> b;
      m_slotsCmd.at(i).deserialize(b);
    }
  }

  AddOnlyProcessToInterval m_addProcessCmd;
  std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;
};

class SCORE_PLUGIN_SCENARIO_EXPORT CreateAutomationFromStates final
    : public CreateProcessAndLayers<Automation::ProcessModel>
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateAutomationFromStates,
      "CreateCurveFromStates")
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

  Curve::CurveDomain m_dom;
  bool m_tween;
};

class SCORE_PLUGIN_SCENARIO_EXPORT CreateInterpolationFromStates final
    : public CreateProcessAndLayers<Interpolation::ProcessModel>
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateInterpolationFromStates,
      "CreateInterpolationFromStates")
public:
  CreateInterpolationFromStates(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> curveId, State::AddressAccessor address,
      ossia::value start, ossia::value end);

  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;
  ossia::value m_start, m_end;
};
}
}
