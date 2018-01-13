#pragma once
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <QByteArray>
#include <QObject>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>
#include <vector>


#include <score/application/ApplicationContext.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT AddProcessToInterval final : public score::Command
{
    SCORE_COMMAND_DECL(
        ScenarioCommandFactoryName(),
        AddProcessToInterval,
        "Add a process to a interval")

  public:
  AddProcessToInterval(
      const IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& process,
      const QString& dat);
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

class AddProcessInNewBoxMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewBoxMacro,
      "Add a process in a new box")

  public:
    ~AddProcessInNewBoxMacro();
};
}
}

