#pragma once
#include <QByteArray>
#include <QMap>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Tools/dataStructures.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore_plugin_scenario_export.h>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class ConstraintModel;
namespace Command
{
/**
         * @brief The ClearConstraint class
         *
         * Removes all the processes and the rackes of a constraint.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT ClearConstraint final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ClearConstraint, "Clear a constraint")
public:
  ClearConstraint(const ConstraintModel& constraintPath);
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  ConstraintSaveData m_constraintSaveData;
};
}
}
