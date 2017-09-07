#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
         * @brief The SetRigidity class
         *
         * Sets the rigidity of a interval
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT SetRigidity final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetRigidity, "Change interval rigidity")

public:
  SetRigidity(const IntervalModel& interval, bool rigid);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  // Unused if the interval was rigid // NOTE Why ??
  TimeVal m_oldMinDuration;
  TimeVal m_oldMaxDuration;

  bool m_rigidity{}, m_oldIsNull{}, m_oldIsInfinite{};
};
}
}
