#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

#include <boost/range/algorithm.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
ScenarioModel::ScenarioModel(const TimeValue& duration,
                             const id_type<ProcessModel>& id,
                             QObject* parent) :
    ProcessModel {duration, id, "ScenarioModel", parent},
    m_startTimeNodeId{0},
    m_endTimeNodeId{1},
    m_startEventId{0},
    m_endEventId{1}
{
    auto& start_tn = CreateTimeNodeMin::redo(m_startTimeNodeId, TimeValue(ZeroTime{}), 0.5, *this);
    auto& end_tn = CreateTimeNodeMin::redo(m_endTimeNodeId, duration, 0.5, *this);

    CreateEventMin::redo(m_startEventId, start_tn, 0., *this);
    CreateEventMin::redo(m_endEventId, end_tn, 0., *this);

    // At the end because plug-ins depend on the start/end timenode & al being here
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this};
}

ScenarioModel::ScenarioModel(const ScenarioModel& source,
                             const id_type<ProcessModel>& id,
                             QObject* parent) :
    ProcessModel {source, id, "ScenarioModel", parent},
    m_startTimeNodeId{source.m_startTimeNodeId},
    m_endTimeNodeId{source.m_endTimeNodeId},
    m_startEventId{source.m_startEventId},
    m_endEventId{source.m_endEventId}
{
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);

    for(ConstraintModel* constraint : source.m_constraints)
    {
        addConstraint(new ConstraintModel {*constraint, constraint->id(), this});
    }

    for(EventModel* event : source.m_events)
    {
        addEvent(new EventModel {*event, event->id(), this});
    }

    for(TimeNodeModel* timenode : source.m_timeNodes)
    {
        addTimeNode(new TimeNodeModel {*timenode, timenode->id(), this});
    }

}

ScenarioModel* ScenarioModel::clone(
        const id_type<ProcessModel>& newId,
        QObject* newParent)
{
    return new ScenarioModel {*this, newId, newParent};
}

QByteArray ScenarioModel::makeViewModelConstructionData() const
{
    // For all existing constraints we need to generate corresponding
    // view models ids. One day we may need to do this for events / time nodes too.
    QMap<id_type<ConstraintModel>, id_type<AbstractConstraintViewModel>> map;
    QVector<id_type<AbstractConstraintViewModel>> vec;
    for(const auto& constraint : m_constraints)
    {
        auto id = getStrongId(vec);
        vec.push_back(id);
        map.insert(constraint->id(), id);
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
    QMap<id_type<ConstraintModel>, id_type<AbstractConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto scen = new TemporalScenarioViewModel {viewModelId, map, *this, parent};
    makeLayer_impl(scen);
    return scen;
}


LayerModel* ScenarioModel::cloneLayer_impl(
        const id_type<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto scen = new TemporalScenarioViewModel{
                static_cast<const TemporalScenarioViewModel&>(source),
                newId,
                *this,
                parent};
    makeLayer_impl(scen);
    return scen;
}

void ScenarioModel::setDurationAndScale(const TimeValue& newDuration)
{
    double scale =  newDuration / duration();

    // Is it recursive ?? Make a scale() method on the constraint, maybe ?
    // TODO we should only have to set the date of the event / constraint
    for(TimeNodeModel* timenode : m_timeNodes)
    {
        timenode->setDate(timenode->date() * scale);
        // Since events will also move we do not need
        // to move the timenode.
    }

    for(EventModel* event : m_events)
    {
        event->setDate(event->date() * scale);
        emit eventMoved(event->id());
    }

    for(ConstraintModel* constraint : m_constraints)
    {
        constraint->setStartDate(constraint->startDate() * scale);
        // Note : scale the min / max.

        ConstraintModel::Algorithms::changeAllDurations(*constraint,
                                                        constraint->defaultDuration() * scale);

        for(auto& process : constraint->processes())
        {
            process->setDurationAndScale(constraint->defaultDuration() * scale);
        }

        emit constraintMoved(constraint->id());
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
    ///* Should work but does not ?
    /* StandardDisplacementPolicy::setEventPosition(*this,
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

Selection ScenarioModel::selectableChildren() const
{
    Selection objects;
    for(const auto& elt : m_constraints)
        objects.insert(elt);
    for(const auto& elt : m_events)
        objects.insert(elt);
    for(const auto& elt : m_timeNodes)
        objects.insert(elt);

    return objects;
}

template<typename InputVec, typename OutputVec>
static void copySelected(const InputVec& in, OutputVec& out)
{
    for(const auto& elt : in)
    {
        if(elt->selection.get())
            out.insert(elt);
    }
}

Selection ScenarioModel::selectedChildren() const
{
    Selection objects;
    copySelected(m_events, objects);
    copySelected(m_timeNodes, objects);
    copySelected(m_constraints, objects);

    return objects;
}

void ScenarioModel::setSelection(const Selection& s) const
{
    // TODO optimize if possible?
    for(auto& elt : m_constraints)
        elt->selection.set(s.find(elt) != s.end());
    for(auto& elt : m_events)
        elt->selection.set(s.find(elt) != s.end());
    for(auto& elt : m_timeNodes)
      elt->selection.set(s.find(elt) != s.end());
}

ProcessStateDataInterface* ScenarioModel::startState() const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

ProcessStateDataInterface* ScenarioModel::endState() const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}


void ScenarioModel::makeLayer_impl(ScenarioModel::layer_type* scen)
{
    // TODO why no ConstraintCreated ?
    connect(this, &ScenarioModel::constraintRemoved,
            scen, &layer_type::on_constraintRemoved);

    connect(this, &ScenarioModel::eventCreated,
            scen, &layer_type::eventCreated);
    connect(this, &ScenarioModel::timeNodeCreated,
            scen, &layer_type::timeNodeCreated);
    connect(this, &ScenarioModel::eventRemoved_after,
            scen, &layer_type::eventDeleted);
    connect(this, &ScenarioModel::timeNodeRemoved,
            scen, &layer_type::timeNodeDeleted);
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

void ScenarioModel::addDisplayedState(DisplayedStateModel *state)
{
    m_displayedStates.insert(state);

}

///////// DELETION //////////
void ScenarioModel::removeConstraint(ConstraintModel* cstr)
{;
    auto constraintId = cstr->id();
    m_constraints.remove(cstr->id());

    emit constraintRemoved(constraintId);
    delete cstr;
}

void ScenarioModel::removeEvent(EventModel* ev)
{
    auto eventId = ev->id();
    emit eventRemoved_before(eventId);

    m_events.remove(ev->id());

    emit eventRemoved_after(eventId);
    delete ev;
}

void ScenarioModel::removeTimeNode(TimeNodeModel* tn)
{
    auto timeNodeId = tn->id();
    m_timeNodes.remove(tn->id());

    emit timeNodeRemoved(timeNodeId);
    delete tn;
}

/////////////////////////////
ConstraintModel& ScenarioModel::constraint(const id_type<ConstraintModel>& constraintId) const
{
    return *m_constraints.at(constraintId);
}

EventModel& ScenarioModel::event(const id_type<EventModel>& eventId) const
{
    return *m_events.at(eventId);
}

TimeNodeModel& ScenarioModel::timeNode(const id_type<TimeNodeModel>& timeNodeId) const
{
    return *m_timeNodes.at(timeNodeId);
}

TimeNodeModel&ScenarioModel::startTimeNode() const
{
    return timeNode(m_startTimeNodeId);
}

TimeNodeModel&ScenarioModel::endTimeNode() const
{
    return timeNode(m_endTimeNodeId);
}

DisplayedStateModel &ScenarioModel::displayedState(const id_type<DisplayedStateModel> &stId) const
{
    return *m_displayedStates.at(stId);
}


EventModel& ScenarioModel::startEvent() const
{
    return event(m_startEventId);
}

EventModel& ScenarioModel::endEvent() const
{
    return event(m_endEventId);
}
