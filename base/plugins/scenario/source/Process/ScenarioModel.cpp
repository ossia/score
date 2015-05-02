#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

ScenarioModel::ScenarioModel(const TimeValue& duration,
                             const id_type<ProcessSharedModelInterface>& id,
                             QObject* parent) :
    ProcessSharedModelInterface {duration, id, "ScenarioModel", parent},
    m_startEventId{0}, // Always
    m_endEventId{1}
{
    auto& start_tn = CreateTimeNodeMin::redo(id_type<TimeNodeModel>(0), TimeValue(ZeroTime{}), 0.5, *this);
    auto& end_tn = CreateTimeNodeMin::redo(id_type<TimeNodeModel>(1), duration, 0.5, *this);

    CreateEventMin::redo(m_startEventId, start_tn, 0.5, *this);
    CreateEventMin::redo(m_endEventId, end_tn, 0.5, *this);

    // TODO event should not need a date; it shall be set by the timenode.
    // Everything should then depend on the "indicative" date of the time node.

}

ScenarioModel::ScenarioModel(const ScenarioModel& source,
                             const id_type<ProcessSharedModelInterface>& id,
                             QObject* parent) :
    ProcessSharedModelInterface {source.duration(), id, "ScenarioModel", parent}
{
    for(ConstraintModel* constraint : source.m_constraints)
    {
        addConstraint(new ConstraintModel {constraint, constraint->id(), this});
    }

    for(EventModel* event : source.m_events)
    {
        addEvent(new EventModel {event, event->id(), this});
    }

    for(TimeNodeModel* timenode : source.m_timeNodes)
    {
        addTimeNode(new TimeNodeModel {timenode, timenode->id(), this});
    }

    m_startEventId = source.m_startEventId;
    m_endEventId = source.m_endEventId;
}

ProcessSharedModelInterface* ScenarioModel::clone(
        const id_type<ProcessSharedModelInterface>& newId,
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

ProcessViewModelInterface* ScenarioModel::makeViewModel(
        const id_type<ProcessViewModelInterface>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    QMap<id_type<ConstraintModel>, id_type<AbstractConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto scen = new TemporalScenarioViewModel {viewModelId, map, *this, parent};
    makeViewModel_impl(scen);
    return scen;
}


ProcessViewModelInterface* ScenarioModel::cloneViewModel(
        const id_type<ProcessViewModelInterface>& newId,
        const ProcessViewModelInterface& source,
        QObject* parent)
{
    auto scen = new TemporalScenarioViewModel{
                static_cast<const TemporalScenarioViewModel&>(source),
                newId,
                *this,
                parent};
    makeViewModel_impl(scen);
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
                                                 [&] (ProcessSharedModelInterface* p, const TimeValue& t)
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
                                                 [&] (ProcessSharedModelInterface* p, const TimeValue& t)
     { p->expandProcess(ExpandMode::Grow, t); }); */

    auto& eev = endEvent();

    eev.setDate(newDuration);
    timeNode(eev.timeNode()).setDate(newDuration);
    emit eventMoved(eev.id());
    this->setDuration(newDuration);
}

Selection ScenarioModel::selectableChildren() const
{
    using namespace std;
    Selection objects;
    copy(begin(m_constraints), end(m_constraints), back_inserter(objects));
    copy(begin(m_events), end(m_events), back_inserter(objects));
    copy(begin(m_timeNodes), end(m_timeNodes), back_inserter(objects));

    return objects;
}

// Since we don't have c++14 & auto in lambdas..
// TODO do it.
template<typename InputVec, typename OutputVec>
void copySelected(const InputVec& in, OutputVec& out)
{
    using namespace std;
    copy_if(begin(in), end(in), back_inserter(out),
            [](typename InputVec::value_type m)
    {
        return m->selection.get();
    });
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
    for(auto elt : m_constraints)
        elt->selection.set(s.contains(elt));
    for(auto elt : m_events)
        elt->selection.set(s.contains(elt));
    for(auto elt : m_timeNodes)
      elt->selection.set(s.contains(elt));
}

ProcessStateDataInterface* ScenarioModel::startState() const
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return nullptr;
}

ProcessStateDataInterface* ScenarioModel::endState() const
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return nullptr;
}


void ScenarioModel::makeViewModel_impl(ScenarioModel::view_model_type* scen)
{
    // TODO this is dangerous since it has to be put for every process.
    addViewModel(scen);

    connect(scen, &TemporalScenarioViewModel::destroyed,
            this, &ScenarioModel::on_viewModelDestroyed);

    // TODO why no ConstraintCreated ?
    connect(this, &ScenarioModel::constraintRemoved,
            scen, &view_model_type::on_constraintRemoved);

    connect(this, &ScenarioModel::eventCreated,
            scen, &view_model_type::eventCreated);
    connect(this, &ScenarioModel::timeNodeCreated,
            scen, &view_model_type::timeNodeCreated);
    connect(this, &ScenarioModel::eventRemoved,
            scen, &view_model_type::eventDeleted);
    connect(this, &ScenarioModel::timeNodeRemoved,
            scen, &view_model_type::timeNodeDeleted);
    connect(this, &ScenarioModel::eventMoved,
            scen, &view_model_type::eventMoved);
    connect(this, &ScenarioModel::constraintMoved,
            scen, &view_model_type::constraintMoved);
}

///////// ADDITION //////////
void ScenarioModel::addConstraint(ConstraintModel* constraint)
{
    m_constraints.push_back(constraint);

    emit constraintCreated(constraint->id());
}

void ScenarioModel::addEvent(EventModel* event)
{
    m_events.push_back(event);

    emit eventCreated(event->id());
}

void ScenarioModel::addTimeNode(TimeNodeModel* timeNode)
{
    m_timeNodes.push_back(timeNode);

    emit timeNodeCreated(timeNode->id());
}

///////// DELETION //////////
void ScenarioModel::removeConstraint(ConstraintModel* cstr)
{;
    auto constraintId = cstr->id();
    vec_erase_remove_if(m_constraints,
                        [&constraintId](ConstraintModel * model)
    {
        return model->id() == constraintId;
    });

    emit constraintRemoved(constraintId);
    delete cstr;
}

void ScenarioModel::removeEvent(EventModel* ev)
{
    auto eventId = ev->id();

    vec_erase_remove_if(m_events,
                        [&eventId](EventModel * model)
    {
        return model->id() == eventId;
    });

    emit eventRemoved(eventId);
    delete ev;
}

void ScenarioModel::removeTimeNode(TimeNodeModel* tn)
{
    auto timeNodeId = tn->id();
    vec_erase_remove_if(m_timeNodes,
                        [&timeNodeId](TimeNodeModel * model)
    {
        return model->id() == timeNodeId;
    });

    emit timeNodeRemoved(timeNodeId);
    delete tn;
}

/////////////////////////////
ConstraintModel& ScenarioModel::constraint(const id_type<ConstraintModel>& constraintId) const
{
    return *findById(m_constraints, constraintId);
}

EventModel& ScenarioModel::event(const id_type<EventModel>& eventId) const
{
    return *findById(m_events, eventId);
}

TimeNodeModel& ScenarioModel::timeNode(const id_type<TimeNodeModel>& timeNodeId) const
{
    return *findById(m_timeNodes, timeNodeId);
}


EventModel& ScenarioModel::startEvent() const
{
    return event(m_startEventId);
}

EventModel& ScenarioModel::endEvent() const
{
    return event(m_endEventId);
}



void ScenarioModel::on_viewModelDestroyed(QObject* obj)
{
    removeViewModel(static_cast<ProcessViewModelInterface*>(obj));
}
