#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"
#include "Process/Temporal/TemporalScenarioLayerModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

#include <boost/range/algorithm.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Scenario/Creations/CreateState.hpp"
ScenarioModel::ScenarioModel(const TimeValue& duration,
                             const Id<Process>& id,
                             QObject* parent) :
    Process {duration, id, "ScenarioModel", parent},
    m_startTimeNodeId{0},
    m_endTimeNodeId{1},
    m_startEventId{0},
    m_endEventId{1}
{
    auto& start_tn = ScenarioCreate<TimeNodeModel>::redo(m_startTimeNodeId, {{0.2, 0.8}}, TimeValue::zero(), *this);
    auto& end_tn = ScenarioCreate<TimeNodeModel>::redo(m_endTimeNodeId, {{0.2, 0.8}}, duration, *this);

    ScenarioCreate<EventModel>::redo(m_startEventId, start_tn, {{0.4, 0.6}}, *this);
    ScenarioCreate<EventModel>::redo(m_endEventId, end_tn, {{0.4, 0.6}}, *this);

    // At the end because plug-ins depend on the start/end timenode & al being here
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this};
}

ScenarioModel::ScenarioModel(const ScenarioModel& source,
                             const Id<Process>& id,
                             QObject* parent) :
    Process {source, id, "ScenarioModel", parent},
    m_startTimeNodeId{source.m_startTimeNodeId},
    m_endTimeNodeId{source.m_endTimeNodeId},
    m_startEventId{source.m_startEventId},
    m_endEventId{source.m_endEventId}
{
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);

    for(const auto& timenode : source.timeNodes)
    {
        timeNodes.add(new TimeNodeModel {timenode, timenode.id(), this});
    }

    for(const auto& event : source.events)
    {
        events.add(new EventModel {event, event.id(), this});
    }

    for(const auto& state : source.states)
    {
        states.add(new StateModel{state, state.id(), this});
    }

    for(const auto& constraint : source.constraints)
    {
        constraints.add(new ConstraintModel {constraint, constraint.id(), this});
    }
}

ScenarioModel* ScenarioModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new ScenarioModel {*this, newId, newParent};
}

QByteArray ScenarioModel::makeViewModelConstructionData() const
{
    // For all existing constraints we need to generate corresponding
    // view models ids. One day we may need to do this for events / time nodes too.
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    QVector<Id<ConstraintViewModel>> vec;
    for(const auto& constraint : constraints)
    {
        auto id = getStrongId(vec);
        vec.push_back(id);
        map.insert(constraint.id(), id);
    }

    QByteArray arr;
    QDataStream s{&arr, QIODevice::WriteOnly};
    s << map;
    return arr;
}

LayerModel* ScenarioModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto scen = new TemporalScenarioLayerModel {viewModelId, map, *this, parent};
    makeLayer_impl(scen);
    return scen;
}


LayerModel* ScenarioModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto scen = new TemporalScenarioLayerModel{
                static_cast<const TemporalScenarioLayerModel&>(source),
                newId,
                *this,
                parent};
    makeLayer_impl(scen);
    return scen;
}

void ScenarioModel::setDurationAndScale(const TimeValue& newDuration)
{
    double scale =  newDuration / duration();

    for(auto& timenode : timeNodes)
    {
        timenode.setDate(timenode.date() * scale);
        // Since events will also move we do not need
        // to move the timenode.
    }

    for(auto& event : events)
    {
        event.setDate(event.date() * scale);
        emit eventMoved(event);
    }

    for(auto& constraint : constraints)
    {
        constraint.setStartDate(constraint.startDate() * scale);
        // Note : scale the min / max.

        ConstraintDurations::Algorithms::changeAllDurations(constraint,
                                                        constraint.duration.defaultDuration() * scale);

        for(auto& process : constraint.processes())
        {
            process.setDurationAndScale(constraint.duration.defaultDuration() * scale);
        }

        emit constraintMoved(constraint);
    }

    this->setDuration(newDuration);
}

#include "Algorithms/StandardDisplacementPolicy.hpp"
void ScenarioModel::setDurationAndGrow(const TimeValue& newDuration)
{
    ///* Should work but does not ?
    /*StandardDisplacementPolicy::setEventPosition(*this,
                                                 endEvent()->id(),
                                                 newDuration,
                                                 endEvent()->heightPercentage(),
                                                 [&] (ProcessModel* p, const TimeValue& t)
     { p->expandProcess(ExpandMode::Grow, t); }); */

    auto& eev = endEvent();

    eev.setDate(newDuration);
    timeNode(eev.timeNode()).setDate(newDuration);
    emit eventMoved(eev);
    this->setDuration(newDuration);
}

void ScenarioModel::setDurationAndShrink(const TimeValue& newDuration)
{
    return; // Disabled by Asana

    ///* Should work but does not ?
    /* StandardDisplacementPolicy::setEventPosition(*this,
                                                 endEvent()->id(),
                                                 newDuration,
                                                 endEvent()->heightPercentage(),
                                                 [&] (ProcessModel* p, const TimeValue& t)
     { p->expandProcess(ExpandMode::Grow, t); }); */

    /*
    auto& eev = endEvent();

    eev.setDate(newDuration);
    timeNode(eev.timeNode()).setDate(newDuration);
    emit eventMoved(eev.id());
    this->setDuration(newDuration);
    */
}

void ScenarioModel::reset()
{
    for(auto& constraint : constraints)
    {
        constraint.reset();
    }

    for(auto& event : events)
    {
        event.reset();
    }

    // TODO reset events / states display too
}

Selection ScenarioModel::selectableChildren() const
{
    Selection objects;
    for(const auto& elt : constraints)
        objects.insert(&elt);
    for(const auto& elt : events)
        objects.insert(&elt);
    for(const auto& elt : timeNodes)
        objects.insert(&elt);
    for(const auto& elt : states)
        objects.insert(&elt);

    return objects;
}

template<typename InputVec, typename OutputVec>
static void copySelected(const InputVec& in, OutputVec& out)
{
    for(const auto& elt : in)
    {
        if(elt.selection.get())
            out.insert(&elt);
    }
}

Selection ScenarioModel::selectedChildren() const
{
    Selection objects;
    copySelected(events, objects);
    copySelected(timeNodes, objects);
    copySelected(constraints, objects);
    copySelected(states, objects);

    return objects;
}

void ScenarioModel::setSelection(const Selection& s) const
{
    // TODO optimize if possible?
    for(auto& elt : constraints)
        elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : events)
        elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : timeNodes)
      elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : states)
      elt.selection.set(s.find(&elt) != s.end());
}

ProcessStateDataInterface* ScenarioModel::startState() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface* ScenarioModel::endState() const
{
    ISCORE_TODO;
    return nullptr;
}


void ScenarioModel::makeLayer_impl(AbstractScenarioLayerModel* scen)
{
    // There is no ConstraintCreated connection to the layer,
    // because the constraints view models are created
    // from the commands, since they require ids too.
    con(constraints, &NotifyingMap<ConstraintModel>::removed,
        scen, &AbstractScenarioLayerModel::on_constraintRemoved);

    con(states, &NotifyingMap<StateModel>::added,
        scen, &AbstractScenarioLayerModel::stateCreated);
    con(states, &NotifyingMap<StateModel>::removed,
        scen, &AbstractScenarioLayerModel::stateRemoved);

    con(events, &NotifyingMap<EventModel>::added,
        scen, &AbstractScenarioLayerModel::eventCreated);
    con(events, &NotifyingMap<EventModel>::removed,
        scen, &AbstractScenarioLayerModel::eventRemoved);

    con(timeNodes, &NotifyingMap<TimeNodeModel>::added,
        scen, &AbstractScenarioLayerModel::timeNodeCreated);
    con(timeNodes, &NotifyingMap<TimeNodeModel>::removed,
        scen, &AbstractScenarioLayerModel::timeNodeRemoved);

    connect(this, &ScenarioModel::eventMoved,
            scen, &AbstractScenarioLayerModel::eventMoved);

    connect(this, &ScenarioModel::constraintMoved,
            scen, &AbstractScenarioLayerModel::constraintMoved);
}
