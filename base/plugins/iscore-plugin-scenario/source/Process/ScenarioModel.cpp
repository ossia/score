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
                             const id_type<Process>& id,
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
                             const id_type<Process>& id,
                             QObject* parent) :
    Process {source, id, "ScenarioModel", parent},
    m_startTimeNodeId{source.m_startTimeNodeId},
    m_endTimeNodeId{source.m_endTimeNodeId},
    m_startEventId{source.m_startEventId},
    m_endEventId{source.m_endEventId}
{
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);

    for(const auto& timenode : source.m_timeNodes)
    {
        addTimeNode(new TimeNodeModel {timenode, timenode.id(), this});
    }

    for(const auto& event : source.m_events)
    {
        addEvent(new EventModel {event, event.id(), this});
    }

    for(const auto& state : source.m_states)
    {
        addState(new StateModel{state, state.id(), this});
    }

    for(const auto& constraint : source.m_constraints)
    {
        addConstraint(new ConstraintModel {constraint, constraint.id(), this});
    }
}

ScenarioModel* ScenarioModel::clone(
        const id_type<Process>& newId,
        QObject* newParent) const
{
    return new ScenarioModel {*this, newId, newParent};
}

QByteArray ScenarioModel::makeViewModelConstructionData() const
{
    // For all existing constraints we need to generate corresponding
    // view models ids. One day we may need to do this for events / time nodes too.
    QMap<id_type<ConstraintModel>, id_type<ConstraintViewModel>> map;
    QVector<id_type<ConstraintViewModel>> vec;
    for(const auto& constraint : m_constraints)
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
        const id_type<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    QMap<id_type<ConstraintModel>, id_type<ConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto scen = new TemporalScenarioLayerModel {viewModelId, map, *this, parent};
    makeLayer_impl(scen);
    return scen;
}


LayerModel* ScenarioModel::cloneLayer_impl(
        const id_type<LayerModel>& newId,
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

    for(auto& timenode : m_timeNodes)
    {
        timenode.setDate(timenode.date() * scale);
        // Since events will also move we do not need
        // to move the timenode.
    }

    for(auto& event : m_events)
    {
        event.setDate(event.date() * scale);
        emit eventMoved(event.id());
    }

    for(auto& constraint : m_constraints)
    {
        constraint.setStartDate(constraint.startDate() * scale);
        // Note : scale the min / max.

        ConstraintModel::Algorithms::changeAllDurations(constraint,
                                                        constraint.defaultDuration() * scale);

        for(auto& process : constraint.processes())
        {
            process.setDurationAndScale(constraint.defaultDuration() * scale);
        }

        emit constraintMoved(constraint.id());
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
    emit eventMoved(eev.id());
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
    for(auto& constraint : m_constraints)
    {
        constraint.reset();
    }

    for(auto& event : m_events)
    {
        event.reset();
    }

    // TODO reset events / states display too
}

Selection ScenarioModel::selectableChildren() const
{
    Selection objects;
    for(const auto& elt : m_constraints)
        objects.insert(&elt);
    for(const auto& elt : m_events)
        objects.insert(&elt);
    for(const auto& elt : m_timeNodes)
        objects.insert(&elt);
    for(const auto& elt : m_states)
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
    copySelected(m_events, objects);
    copySelected(m_timeNodes, objects);
    copySelected(m_constraints, objects);
    copySelected(m_states, objects);

    return objects;
}

void ScenarioModel::setSelection(const Selection& s) const
{
    // TODO optimize if possible?
    for(auto& elt : m_constraints)
        elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : m_events)
        elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : m_timeNodes)
      elt.selection.set(s.find(&elt) != s.end());
    for(auto& elt : m_states)
      elt.selection.set(s.find(&elt) != s.end());
}

ProcessStateDataInterface* ScenarioModel::startState() const
{
    ISCORE_TODO
    return nullptr;
}

ProcessStateDataInterface* ScenarioModel::endState() const
{
    ISCORE_TODO
    return nullptr;
}


void ScenarioModel::makeLayer_impl(ScenarioModel::layer_type* scen)
{
    // There is no ConstraintCreated connection to the layer,
    // because the constraints view models are created
    // from the commands, since they require ids too.
    connect(this, &ScenarioModel::constraintRemoved,
            scen, &layer_type::on_constraintRemoved);

    connect(this, &ScenarioModel::stateCreated,
            scen, &layer_type::stateCreated);
    connect(this, &ScenarioModel::stateRemoved,
            scen, &layer_type::stateRemoved);

    connect(this, &ScenarioModel::eventCreated,
            scen, &layer_type::eventCreated);
    connect(this, &ScenarioModel::eventRemoved_after,
            scen, &layer_type::eventRemoved);

    connect(this, &ScenarioModel::timeNodeCreated,
            scen, &layer_type::timeNodeCreated);
    connect(this, &ScenarioModel::timeNodeRemoved,
            scen, &layer_type::timeNodeRemoved);

    connect(this, &ScenarioModel::eventMoved,
            scen, &layer_type::eventMoved);

    connect(this, &ScenarioModel::constraintMoved,
            scen, &layer_type::constraintMoved);
}

///////// ADDITION //////////
void ScenarioModel::addConstraint(ConstraintModel* constraint)
{
    m_constraints.insert(constraint);

    emit constraintCreated(constraint->id());
}

void ScenarioModel::addEvent(EventModel* event)
{
    m_events.insert(event);

    emit eventCreated(event->id());
}

void ScenarioModel::addTimeNode(TimeNodeModel* timeNode)
{
    m_timeNodes.insert(timeNode);

    emit timeNodeCreated(timeNode->id());
}

void ScenarioModel::addState(StateModel *state)
{
    m_states.insert(state);

    emit stateCreated(state->id());
}

///////// DELETION //////////
void ScenarioModel::removeConstraint(ConstraintModel* cstr)
{
    const auto& constraintId = cstr->id();
    m_constraints.remove(constraintId);

    emit constraintRemoved(constraintId);
    delete cstr;
}

void ScenarioModel::removeEvent(EventModel* ev)
{
    auto eventId = ev->id(); // Copy because it gets deleted.
    emit eventRemoved_before(eventId);

    m_events.remove(eventId);

    emit eventRemoved_after(eventId);
    delete ev;
}

void ScenarioModel::removeTimeNode(TimeNodeModel* tn)
{
    const auto& timeNodeId = tn->id();
    m_timeNodes.remove(timeNodeId);

    emit timeNodeRemoved(timeNodeId);
    delete tn;
}

void ScenarioModel::removeState(StateModel *state)
{
    const auto& id = state->id();
    emit stateRemoved(id);

    m_states.remove(id);
    delete state;
}
