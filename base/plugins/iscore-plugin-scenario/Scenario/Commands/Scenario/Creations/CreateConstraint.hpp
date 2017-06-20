#pragma once
#include <QString>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class StateModel;
class ConstraintModel;
class ProcessModel;

namespace Command
{
/**
        * @brief The CreateEventAfterEventCommand class
        *
        * This Command creates a constraint and another event in a scenario,
        * starting from an event selected by the user.
        */
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateConstraint final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateConstraint, "Create a constraint")
public:
  CreateConstraint(
      const Scenario::ProcessModel& scenarioPath,
      Id<StateModel>
          startState,
      Id<StateModel>
          endState);
  CreateConstraint& operator=(CreateConstraint&&) = default;

  const Path<Scenario::ProcessModel>& scenarioPath() const
  {
    return m_path;
  }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  const Id<ConstraintModel>& createdConstraint() const
  {
    return m_createdConstraintId;
  }

  const Id<StateModel>& startState() const
  {
    return m_startStateId;
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  QString m_createdName;

  Id<ConstraintModel> m_createdConstraintId{};

  Id<StateModel> m_startStateId{};
  Id<StateModel> m_endStateId{};
};
}
}
