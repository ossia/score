// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExtendSequence.hpp"

#include "AppendSequenceSection.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <Process/ExpandMode.hpp>

namespace Sequence
{
namespace Command
{

ExtendSequence* ExtendSequence::make(
    const Scenario::ProcessModel& scenario,
    const Id<Scenario::StateModel>& endStateId,
    const TimeVal& newDate,
    double y)
{
  auto& endState = scenario.states.at(endStateId);
  SCORE_ASSERT(endState.previousInterval());
  auto& parentItv = scenario.intervals.at(*endState.previousInterval());

  // Find the SequenceModel inside the parent interval
  Sequence::SequenceModel* seqModel = nullptr;
  for(auto& proc : parentItv.processes)
  {
    seqModel = qobject_cast<Sequence::SequenceModel*>(&proc);
    if(seqModel)
      break;
  }
  SCORE_ASSERT(seqModel);

  const auto& endEvent = scenario.events.at(endState.eventId());
  const TimeVal sectionDuration = newDate - endEvent.date();
  if(sectionDuration <= TimeVal::zero())
    return nullptr;

  auto cmd = new ExtendSequence;

  // 1. Move the parent interval's end event to newDate
  cmd->addCommand(new Scenario::Command::MoveEventMeta{
      scenario, endState.eventId(), newDate, y, ExpandMode::GrowShrink, LockMode::Free});

  // 2. Append a new section inside the SequenceModel
  cmd->addCommand(new AppendSequenceSection{*seqModel, sectionDuration});

  return cmd;
}

}
}
