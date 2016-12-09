#pragma once
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Address.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <utility>
#include <vector>

#include <Automation/AutomationProcessMetadata.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Process/ProcessList.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class LayerModel;
}
namespace Process
{
class ProcessModel;
}
template <typename Object>
class Path;

namespace Scenario
{
class ConstraintModel;
class SlotModel;
namespace Command
{
template <typename ProcessModel_T>
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateProcessAndLayers
    : public iscore::Command
{
public:
  CreateProcessAndLayers() = default;
  CreateProcessAndLayers(
      Path<ConstraintModel>&& constraint,
      const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>&
          slotList,
      Id<Process::ProcessModel>
          procId)
      : m_addProcessCmd{std::move(constraint), std::move(procId),
                        Metadata<ConcreteKey_k, ProcessModel_T>::get()}
  {
    auto proc = m_addProcessCmd.constraintPath().extend(
        Metadata<ObjectKey_k, ProcessModel_T>::get(), procId);

    m_slotsCmd.reserve(slotList.size());

    auto fact = context.interfaces<Process::LayerFactoryList>()
                    .findDefaultFactory(
                        Metadata<ConcreteKey_k, ProcessModel_T>::get());
    ISCORE_ASSERT(fact);
    auto procData = fact->makeStaticLayerConstructionData();

    for (const auto& elt : slotList)
    {
      m_slotsCmd.emplace_back(
          Path<SlotModel>(elt.first),
          elt.second,
          Path<Process::ProcessModel>{proc},
          fact->concreteKey(),
          procData);
    }
  }

  void undo() const final override
  {
    for (const auto& cmd : m_slotsCmd)
      cmd.undo();
    m_addProcessCmd.undo();
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

  AddOnlyProcessToConstraint m_addProcessCmd;
  std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT CreateAutomationFromStates final
    : public CreateProcessAndLayers<Automation::ProcessModel>
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateAutomationFromStates,
      "CreateCurveFromStates")
public:
  CreateAutomationFromStates(
      const ConstraintModel& constraint,
      const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>&
          slotList,
      Id<Process::ProcessModel> curveId, State::AddressAccessor address,
      double start, double end, double min, double max);

  void redo() const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;

  double m_start{}, m_end{};
  double m_min{}, m_max{};
};

class ISCORE_PLUGIN_SCENARIO_EXPORT CreateInterpolationFromStates final
    : public CreateProcessAndLayers<Interpolation::ProcessModel>
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateInterpolationFromStates,
      "CreateInterpolationFromStates")
public:
  CreateInterpolationFromStates(
      const ConstraintModel& constraint,
      const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>&
          slotList,
      Id<Process::ProcessModel> curveId, State::AddressAccessor address,
      State::Value start, State::Value end);

  void redo() const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  State::AddressAccessor m_address;
  State::Value m_start, m_end;
};
}
}
