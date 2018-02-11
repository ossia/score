// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <algorithm>
#include <score/tools/std/Optional.hpp>
#include <qnamespace.h>
#include <tuple>

#include "Loop/LoopProcessMetadata.hpp"
#include "LoopProcessModel.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/Identifier.hpp>

namespace Loop
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
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

const QVector<Id<Scenario::IntervalModel>> intervalsBeforeTimeSync(
    const ProcessModel& scen, const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  if (timeSyncId == scen.endTimeSync().id())
  {
    return {scen.interval().id()};
  }
  return {};
}
}
