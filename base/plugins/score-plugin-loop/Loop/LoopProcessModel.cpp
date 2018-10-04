// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "LoopProcessModel.hpp"

#include "Loop/LoopProcessMetadata.hpp"

#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/algorithms.hpp>

#include <qnamespace.h>

#include <wobjectimpl.h>

#include <algorithm>
#include <tuple>
W_OBJECT_IMPL(Loop::ProcessModel)
namespace Loop
{
ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , Scenario::BaseScenarioContainer{this}
    , inlet{Process::make_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  Scenario::IntervalDurations::Algorithms::changeAllDurations(
      interval(), duration);
  endEvent().setDate(duration);
  endTimeSync().setDate(duration);

  const double height = 0.5;
  interval().setHeightPercentage(height);
  interval().metadata().setName("pattern");
  BaseScenarioContainer::startState().setHeightPercentage(height);
  BaseScenarioContainer::endState().setHeightPercentage(height);
  BaseScenarioContainer::startEvent().setExtent({height, 0.2});
  BaseScenarioContainer::endEvent().setExtent({height, 0.2});
  BaseScenarioContainer::startTimeSync().setExtent({height, 1});
  BaseScenarioContainer::endTimeSync().setExtent({height, 1});

  metadata().setInstanceName(*this);

  inlet->type = Process::PortType::Audio;
  outlet->type = Process::PortType::Audio;
  outlet->setPropagate(true);
  init();
}

ProcessModel::~ProcessModel()
{
  identified_object_destroying(this);
}

void ProcessModel::startExecution()
{
  interval().startExecution();
}

void ProcessModel::stopExecution()
{
  interval().stopExecution();
}

void ProcessModel::reset()
{
  interval().reset();
  startEvent().setStatus(Scenario::ExecutionStatus::Editing, *this);
  endEvent().setStatus(Scenario::ExecutionStatus::Editing, *this);
}

Selection ProcessModel::selectableChildren() const
{
  Selection s;

  ossia::for_each_in_tuple(elements(), [&](auto elt) { s.append(elt); });

  return s;
}

Selection ProcessModel::selectedChildren() const
{
  Selection s;

  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    if (elt->selection.get())
      s.append(elt);
  });

  return s;
}

void ProcessModel::setSelection(const Selection& s) const
{
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    elt->selection.set(s.contains(elt)); // OPTIMIZEME
  });
}

/*
void ProcessModel::changeDuration(
    Scenario::IntervalModel& itv, const TimeVal& v)
{
  Scenario::Command::MoveBaseEvent<Loop::ProcessModel> cmd(
      *this, endEvent().id(), v, 0., ExpandMode::GrowShrink, LockMode::Free);
  cmd.redo(score::IDocument::documentContext(*this));
}

void ProcessModel::changeDuration(
    const Scenario::IntervalModel& itv, OngoingCommandDispatcher& dispatcher,
    const TimeVal& val, ExpandMode expandmode, LockMode lockmode)
{
  auto& loop = *this;
  dispatcher
      .submit<Scenario::Command::MoveBaseEvent<Loop::ProcessModel>>(
          loop, loop.state(itv.endState()).eventId(), itv.date() + val, 0,
          expandmode, lockmode);
}
*/

const QVector<Id<Scenario::IntervalModel>> intervalsBeforeTimeSync(
    const ProcessModel& scen, const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  if (timeSyncId == scen.endTimeSync().id())
  {
    return {scen.interval().id()};
  }
  return {};
}


bool LoopIntervalResizer::matches(const Scenario::IntervalModel& interval) const noexcept
{
  return dynamic_cast<Loop::ProcessModel*>(interval.parent());
}

score::Command* LoopIntervalResizer::make(
    const Scenario::IntervalModel& interval, TimeVal new_duration, ExpandMode e, LockMode l) const noexcept
{
  auto scenar = dynamic_cast<Loop::ProcessModel*>(interval.parent());
  if(!scenar)
    return nullptr;

  return new Scenario::Command::MoveBaseEvent<Loop::ProcessModel>{
          *scenar,
          scenar->endEvent().id(),
          new_duration,
          interval.heightPercentage(),
        e, l};
}

void LoopIntervalResizer::update(
      score::Command& cmd, const Scenario::IntervalModel& interval,
      TimeVal new_duration, ExpandMode e, LockMode l) const noexcept
{
  auto c = dynamic_cast<Scenario::Command::MoveBaseEvent<Loop::ProcessModel>*>(&cmd);
  if(c)
  {
    auto scenar = dynamic_cast<Loop::ProcessModel*>(interval.parent());
    auto& ev = Scenario::endState(interval, *scenar).eventId();
    c->update(*scenar, ev, interval.date() + new_duration, interval.heightPercentage(), e, l);
  }
}

}
