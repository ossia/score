// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>
#include <qnamespace.h>
#include <tuple>

#include "Loop/LoopProcessMetadata.hpp"
#include "LoopProcessModel.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/Skin.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/model/Identifier.hpp>

namespace Loop
{

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , Scenario::BaseScenarioContainer{this}
{
  Scenario::ConstraintDurations::Algorithms::changeAllDurations(
      constraint(), duration);
  endEvent().setDate(duration);
  endTimeSync().setDate(duration);

  const double height = 0.5;
  constraint().setHeightPercentage(height);
  constraint().metadata().setName("pattern");
  BaseScenarioContainer::startState().setHeightPercentage(height);
  BaseScenarioContainer::endState().setHeightPercentage(height);
  BaseScenarioContainer::startEvent().setExtent({height, 0.2});
  BaseScenarioContainer::endEvent().setExtent({height, 0.2});
  BaseScenarioContainer::startTimeSync().setExtent({height, 1});
  BaseScenarioContainer::endTimeSync().setExtent({height, 1});

  metadata().setInstanceName(*this);
}

ProcessModel::ProcessModel(
    const Loop::ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{source, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , BaseScenarioContainer{source, this}
{
}

ProcessModel::~ProcessModel()
{
  emit identified_object_destroying(this);
}

void ProcessModel::startExecution()
{
  constraint().startExecution();
}

void ProcessModel::stopExecution()
{
  constraint().stopExecution();
}

void ProcessModel::reset()
{
  constraint().reset();
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

const QVector<Id<Scenario::ConstraintModel>> constraintsBeforeTimeSync(
    const ProcessModel& scen, const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  if (timeSyncId == scen.endTimeSync().id())
  {
    return {scen.constraint().id()};
  }
  return {};
}
}
