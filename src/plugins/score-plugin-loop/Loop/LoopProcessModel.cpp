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

#include <tuple>
W_OBJECT_IMPL(Loop::ProcessModel)
namespace Loop
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , Scenario::BaseScenarioContainer{ctx, this}
    , inlet{Process::make_audio_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_audio_outlet(Id<Process::Port>(0), this)}
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

  metadata().setInstanceName(*this);

  outlet->setPropagate(true);
  init();
}

void ProcessModel::init()
{
  m_inlets.push_back(inlet.get());
  m_outlets.push_back(outlet.get());
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

Selection ProcessModel::selectableChildren() const noexcept
{
  Selection s;

  ossia::for_each_in_tuple(elements(), [&](auto elt) { s.append(elt); });

  return s;
}

Selection ProcessModel::selectedChildren() const noexcept
{
  Selection s;

  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    if (elt->selection.get())
      s.append(elt);
  });

  return s;
}

void ProcessModel::setSelection(const Selection& s) const noexcept
{
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    elt->selection.set(s.contains(elt)); // OPTIMIZEME
  });
}

const QVector<Id<Scenario::IntervalModel>> intervalsBeforeTimeSync(
    const ProcessModel& scen,
    const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  if (timeSyncId == scen.endTimeSync().id())
  {
    return {scen.interval().id()};
  }
  return {};
}

bool LoopIntervalResizer::matches(
    const Scenario::IntervalModel& interval) const noexcept
{
  return dynamic_cast<Loop::ProcessModel*>(interval.parent());
}

score::Command* LoopIntervalResizer::make(
    const Scenario::IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto scenar = dynamic_cast<Loop::ProcessModel*>(interval.parent());
  if (!scenar)
    return nullptr;

  return new Scenario::Command::MoveBaseEvent<Loop::ProcessModel>{
      *scenar,
      scenar->endEvent().id(),
      new_duration,
      interval.heightPercentage(),
      e,
      l};
}

void LoopIntervalResizer::update(
    score::Command& cmd,
    const Scenario::IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto c = dynamic_cast<Scenario::Command::MoveBaseEvent<Loop::ProcessModel>*>(
      &cmd);
  if (c)
  {
    auto scenar = dynamic_cast<Loop::ProcessModel*>(interval.parent());
    auto& ev = Scenario::endState(interval, *scenar).eventId();
    c->update(
        *scenar,
        ev,
        interval.date() + new_duration,
        interval.heightPercentage(),
        e,
        l);
  }
}

}
