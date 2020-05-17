#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/Interval/SetRigidity.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/SplitTimeSync.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <QPair>
#include <QVector>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;
class EventModel;
class StateModel;
class TimeSyncModel;
class CommentBlockModel;
class ProcessModel;
namespace Command
{
/**
 * @brief The RemoveSelection class
 *
 * Tries to remove what is selected in a scenario.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT RemoveSelection final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveSelection, "Remove selected elements")
public:
  RemoveSelection(const Scenario::ProcessModel&, Selection sel);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;

  QVector<QPair<Id<CommentBlockModel>, QByteArray>> m_removedComments;
  QVector<QPair<Id<StateModel>, QByteArray>> m_removedStates;
  QVector<QPair<Id<EventModel>, QByteArray>> m_cleanedEvents;
  QVector<QPair<Id<TimeSyncModel>, QByteArray>> m_cleanedTimeSyncs;
  QVector<QPair<Id<IntervalModel>, QByteArray>> m_removedIntervals;
  std::vector<SetRigidity> m_cmds_set_rigidity;
  Dataflow::SerializedCables m_cables;
};
}
}
