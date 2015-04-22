#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

#include <QDebug>


ScenarioModel::ScenarioModel(TimeValue duration,
                             id_type<ProcessSharedModelInterface> id,
                             QObject* parent) :
    ProcessSharedModelInterface {duration, id, "ScenarioModel", parent},
    //m_scenario{nullptr},
    m_startEventId{0}, // Always
    m_endEventId{1}
{
    auto& start_tn = CreateTimeNodeMin::redo(id_type<TimeNodeModel>(0), TimeValue(ZeroTime{}), 0.5, *this);
    auto& end_tn = CreateTimeNodeMin::redo(id_type<TimeNodeModel>(1), duration, 0.5, *this);

    CreateEventMin::redo(m_startEventId, start_tn, 0.5, *this);
    CreateEventMin::redo(m_endEventId, end_tn, 0.5, *this);

    // TODO event should not need a date; it shall be set by the timenode

}

ProcessSharedModelInterface* ScenarioModel::clone(id_type<ProcessSharedModelInterface> newId, QObject* newParent)
{
    auto scenario = new ScenarioModel {this->duration(), newId, newParent};

    for(ConstraintModel* constraint : m_constraints)
    {
        scenario->addConstraint(new ConstraintModel {constraint, constraint->id(), scenario});
    }

    for(EventModel* event : m_events)
    {
        scenario->addEvent(new EventModel {event, event->id(), scenario});
    }

    for(TimeNodeModel* timenode : m_timeNodes)
    {
        scenario->addTimeNode(new TimeNodeModel {timenode, timenode->id(), scenario});
    }

    scenario->m_startEventId = m_startEventId;
    scenario->m_endEventId = m_endEventId;

    return scenario;
}

ProcessViewModelInterface* ScenarioModel::makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
                                                        QObject* parent)
{
    auto scen = new TemporalScenarioViewModel {viewModelId, this, parent};
    makeViewModel_impl(scen);
    return scen;
}


ProcessViewModelInterface* ScenarioModel::makeViewModel(id_type<ProcessViewModelInterface> newId,
                                                        const ProcessViewModelInterface* source,
                                                        QObject* parent)
{
    auto scen = new TemporalScenarioViewModel
    {
                static_cast<const TemporalScenarioViewModel*>(source),
                newId,
                this,
                parent
};
    makeViewModel_impl(scen);
    return scen;
}

void ScenarioModel::setDurationAndScale(TimeValue newDuration)
{
    double scale =  newDuration / duration();

    // Is it recursive ?? Make a scale() method on the constraint, maybe ?
    // TODO we should only have to set the date of the event / constraint
    for(TimeNodeModel* timenode : m_timeNodes)
    {
        timenode->setDate(timenode->date() * scale);
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
        emit constraintMoved(constraint->id());
    }

    this->setDuration(newDuration);
}

void ScenarioModel::setDurationAndGrow(TimeValue newDuration)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    // Here and in setDurationAndShrink, this should "drag" the last timenode.
}

void ScenarioModel::setDurationAndShrink(TimeValue newDuration)
{
    qDebug() << Q_FUNC_INFO << "TODO";
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

void ScenarioModel::setSelection(const Selection& s)
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

// TODO if we go pass-by-value, use std::move here.
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
#include <iscore/tools/utilsCPP11.hpp>
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
ConstraintModel* ScenarioModel::constraint(id_type<ConstraintModel> constraintId) const
{
    return findById(m_constraints, constraintId);
}

EventModel* ScenarioModel::event(id_type<EventModel> eventId) const
{
    return findById(m_events, eventId);
}

TimeNodeModel* ScenarioModel::timeNode(id_type<TimeNodeModel> timeNodeId) const
{
    return findById(m_timeNodes, timeNodeId);
}


EventModel* ScenarioModel::startEvent() const
{
    return event(m_startEventId);
}

EventModel* ScenarioModel::endEvent() const
{
    return event(m_endEventId);
}



void ScenarioModel::on_viewModelDestroyed(QObject* obj)
{
    removeViewModel(static_cast<ProcessViewModelInterface*>(obj));
}
