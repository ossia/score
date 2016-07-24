#include <Loop/LoopLayer.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <qnamespace.h>
#include <algorithm>
#include <tuple>

#include "Loop/LoopProcessMetadata.hpp"
#include "LoopProcessModel.hpp"
#include <Process/Style/Skin.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;

namespace Loop
{

ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    Scenario::BaseScenarioContainer{this}
{
    this->setUseParentDuration(true);

    Scenario::ConstraintDurations::Algorithms::changeAllDurations(constraint(), duration);
    endEvent().setDate(duration);
    endTimeNode().setDate(duration);

    const double height = 0.15;
    constraint().setHeightPercentage(height);
    constraint().metadata.setName("Loop pattern");
    constraint().metadata.setColor(ScenarioStyle::instance().ConstraintInvalid);
    BaseScenarioContainer::startState().setHeightPercentage(height);
    BaseScenarioContainer::endState().setHeightPercentage(height);
    BaseScenarioContainer::startEvent().setExtent({height, 0.2});
    BaseScenarioContainer::endEvent().setExtent({height, 0.2});
    BaseScenarioContainer::startTimeNode().setExtent({height, 1});
    BaseScenarioContainer::endTimeNode().setExtent({height, 1});
}

ProcessModel::ProcessModel(
        const Loop::ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{source.duration(), id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    BaseScenarioContainer{source, this}
{
}

ProcessModel::~ProcessModel()
{
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
    startEvent().reset();
    endEvent().reset();
    // TODO reset events / states display too
}

Selection ProcessModel::selectableChildren() const
{
    Selection s;

    for_each_in_tuple(elements(), [&] (auto elt) { s.append(elt); });

    return s;
}

Selection ProcessModel::selectedChildren() const
{
    Selection s;

    for_each_in_tuple(elements(), [&] (auto elt) {
        if(elt->selection.get())
            s.append(elt);
    });

    return s;
}

void ProcessModel::setSelection(const Selection& s) const
{
    for_each_in_tuple(elements(), [&] (auto elt) {
        elt->selection.set(s.contains(elt)); // OPTIMIZEME
    });
}

const QVector<Id<Scenario::ConstraintModel> > constraintsBeforeTimeNode(
        const ProcessModel& scen,
        const Id<Scenario::TimeNodeModel>& timeNodeId)
{
    if(timeNodeId == scen.endTimeNode().id())
    {
        return {scen.constraint().id()};
    }
    return {};
}

}
