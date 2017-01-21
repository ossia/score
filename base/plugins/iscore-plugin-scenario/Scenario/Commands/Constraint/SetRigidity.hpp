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
class ConstraintModel;
namespace Command
{
/**
         * @brief The SetRigidity class
         *
         * Sets the rigidity of a constraint
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT SetRigidity final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetRigidity, "Change constraint rigidity")

public:
  SetRigidity(Path<ConstraintModel>&& constraintPath, bool rigid);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_path;
  // Unused if the constraint was rigid // NOTE Why ??
  TimeVal m_oldMinDuration;
  TimeVal m_oldMaxDuration;

  bool m_rigidity{}, m_oldIsNull{}, m_oldIsInfinite{};
};
}
}
