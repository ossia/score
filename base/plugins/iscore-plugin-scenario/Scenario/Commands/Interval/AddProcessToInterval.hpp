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
#include <iscore/command/AggregateCommand.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <QString>
#include <vector>


#include <iscore/application/ApplicationContext.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT AddProcessToInterval final : public iscore::Command
{
    ISCORE_COMMAND_DECL(
        ScenarioCommandFactoryName(),
        AddProcessToInterval,
        "Add a process to a interval")

  public:
  AddProcessToInterval(
      const IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& process);
    ~AddProcessToInterval();

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  const Path<IntervalModel>& intervalPath() const;
  const Id<Process::ProcessModel>& processId() const;
  const UuidKey<Process::ProcessModel>& processKey() const;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  AddOnlyProcessToInterval m_addProcessCommand;
  bool m_addedSlot{};
};

class AddProcessInNewBoxMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewBoxMacro,
      "Add a process in a new box")

  public:
    ~AddProcessInNewBoxMacro();
};
}
}

