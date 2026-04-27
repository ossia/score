#pragma once
#include "AppendSequenceSection.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>

#include <memory>

namespace Scenario
{
class ProcessModel;
class StateModel;
}

namespace Sequence
{
namespace Command
{

// Extends an existing sequence by appending a new section and moving
// the parent interval's end event.
//
// Implemented as an ongoing command (has update()) so that
// MultiOngoingCommandDispatcher can submit once on the first mouse-move frame
// and call update()+redo() on every subsequent frame — no rollback/recreate
// between frames, no flicker.
//
// redo() order: AppendSection first, then MoveEventMeta.
// This ensures setDurationAndGrow() is a no-op (SequenceModel duration already
// equals the new parent duration when MoveEventMeta runs).
class ExtendSequence final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), ExtendSequence, "Extend sequence")
public:
  // Constructor and update() share the same argument list so that the
  // MultiOngoingCommandDispatcher template can forward them to both.
  ExtendSequence(
      const Scenario::ProcessModel& scenario,
      const Id<Scenario::StateModel>& endStateId, const TimeVal& newDate, double y);

  ~ExtendSequence() override;

  // Called by the dispatcher on subsequent mouse-move frames.
  // scenario and endStateId are ignored after construction — accepted only to
  // match the constructor signature required by the dispatcher template.
  void update(
      const Scenario::ProcessModel& scenario,
      const Id<Scenario::StateModel>& endStateId, const TimeVal& newDate, double y);

  void redo(const score::DocumentContext& ctx) const override;
  void undo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  // Stored so update() can forward to m_moveCmd->update() without needing
  // to re-find the event from the state.
  Id<Scenario::EventModel> m_endEventId;

  // Owned sub-commands. Pointers because both have non-trivial construction
  // that requires information gathered in the constructor body.
  std::unique_ptr<AppendSequenceSection> m_appendCmd;
  std::unique_ptr<Scenario::Command::MoveEventMeta> m_moveCmd;

  // True once AppendSection has been applied; reset to false on undo().
  // Prevents re-applying the section on each dispatcher update()+redo() cycle.
  mutable bool m_sectionApplied{false};
};

}
}
