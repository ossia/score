#pragma once
#include <QByteArray>
#include <QPair>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/selection/Selection.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class CommentBlockModel;
class ProcessModel;
namespace Command
{
/**
 * @brief The RemoveSelection class
 *
 * Tries to remove what is selected in a scenario.
 */
class RemoveSelection final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), RemoveSelection,
      "Remove selected elements")
public:
  RemoveSelection(
      const Scenario::ProcessModel&,
      Selection sel);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;

  // For timenodes that may be removed when there is only a single event
  QVector<QPair<Id<TimeNodeModel>, QByteArray>> m_maybeRemovedTimeNodes;

  QVector<QPair<Id<CommentBlockModel>, QByteArray>> m_removedComments;
  QVector<QPair<Id<StateModel>, QByteArray>> m_removedStates;
  QVector<QPair<Id<EventModel>, QByteArray>> m_removedEvents;
  QVector<QPair<Id<TimeNodeModel>, QByteArray>> m_removedTimeNodes;
  QVector<QPair<Id<ConstraintModel>, QByteArray>> m_removedConstraints;
};
}
}
