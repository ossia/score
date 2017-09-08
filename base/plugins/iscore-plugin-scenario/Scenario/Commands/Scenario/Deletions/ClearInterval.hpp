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
class IntervalModel;
namespace Command
{
/**
         * @brief The ClearInterval class
         *
         * Removes all the processes and the rackes of a interval.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT ClearInterval final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ClearInterval, "Clear a interval")
public:
  ClearInterval(const IntervalModel& intervalPath);
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  IntervalSaveData m_intervalSaveData;
};
}
}
