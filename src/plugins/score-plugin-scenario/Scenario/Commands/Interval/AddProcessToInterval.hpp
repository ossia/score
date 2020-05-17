#pragma once
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>

#include <vector>

namespace Scenario
{
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT AddProcessToInterval final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddProcessToInterval, "Add a process to a interval")

public:
  AddProcessToInterval(
      const IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& process,
      const QString& dat,
      const QPointF& pos);
  ~AddProcessToInterval();

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<IntervalModel>& intervalPath() const;
  const Id<Process::ProcessModel>& processId() const;
  const UuidKey<Process::ProcessModel>& processKey() const;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  AddOnlyProcessToInterval m_addProcessCommand;
  bool m_addedSlot{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT LoadProcessInInterval final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), LoadProcessInInterval, "Load a process in an interval")

public:
  LoadProcessInInterval(const IntervalModel& interval, const rapidjson::Value& dat);
  ~LoadProcessInInterval();

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<IntervalModel>& intervalPath() const;
  const Id<Process::ProcessModel>& processId() const;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;
  LoadOnlyProcessInInterval m_addProcessCommand;
  bool m_addedSlot{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT AddProcessInNewBoxMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddProcessInNewBoxMacro, "Add a process in a new box")

public:
  ~AddProcessInNewBoxMacro();
};

class SCORE_PLUGIN_SCENARIO_EXPORT DropProcessInIntervalMacro final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      DropProcessInIntervalMacro,
      "Drop a process in an interval")
};
}
}
