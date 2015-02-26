#include "ScenarioModel.hpp"

#include "Process/Temporal/TemporalScenarioViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>

ScenarioModel::ScenarioModel(id_type<ProcessSharedModelInterface> id, QObject* parent) :
    ProcessSharedModelInterface {id, "ScenarioModel", parent},
                            //m_scenario{nullptr},
m_startEventId {0} // Always
{
    auto event = new EventModel{m_startEventId, this};
    addEvent(event);

    createTimeNode(id_type<TimeNodeModel> (0), m_startEventId);

    //TODO demander à Clément si l'élément de fin sert vraiment à qqch ?
    //m_events.push_back(new EventModel(1, this));
}

ProcessSharedModelInterface* ScenarioModel::clone(id_type<ProcessSharedModelInterface> newId, QObject* newParent)
{
    auto scenario = new ScenarioModel {newId, newParent};

    for(ConstraintModel* constraint : m_constraints)
    {
        scenario->addConstraint(new ConstraintModel {constraint, constraint->id(), scenario});
    }

    for(EventModel* event : m_events)
    {
        scenario->addEvent(new EventModel {event, event->id(), scenario});
    }

    //TODO addTimeNode ????

    scenario->m_startEventId = m_startEventId;
    scenario->m_endEventId = m_endEventId;

    return scenario;
}

ScenarioModel::~ScenarioModel()
{
    //if(m_scenario) delete m_scenario;
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

void ScenarioModel::setDurationWithScale(TimeValue newDuration)
{
    auto scale =  double(newDuration.msec()) / duration().msec();
    // Is it recursive ?? Make a scale() method on the constraint, maybe ?

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
        constraint->setDefaultDuration(constraint->defaultDuration() * scale);
        emit constraintMoved(constraint->id());
    }

    this->setDuration(newDuration);
}

void ScenarioModel::setDurationWithoutScale(TimeValue newDuration)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    // There should be nothing to do here ?
    // Maybe with the last event/timenode ? (hence they finally have a meaning)
}


void ScenarioModel::makeViewModel_impl(ScenarioModel::view_model_type* scen)
{
    addViewModel(scen);

    connect(scen, &TemporalScenarioViewModel::destroyed,
            this, &ScenarioModel::on_viewModelDestroyed);

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


//////// Creation ////////
void ScenarioModel::createConstraintBetweenEvents(id_type<EventModel> startEventId,
        id_type<EventModel> endEventId,
        id_type<ConstraintModel> newConstraintModelId,
        id_type<AbstractConstraintViewModel> newConstraintFullViewId)
{
    auto sev = this->event(startEventId);
    auto eev = this->event(endEventId);
    auto constraint = new ConstraintModel {newConstraintModelId,
                                           newConstraintFullViewId,
                                           this
                                          };

    /*	auto ossia_tn0 = sev->apiObject();
    auto ossia_tn1 = eev->apiObject();
    auto ossia_tb = inter->apiObject();

    m_scenario->addTimeBox(*ossia_tb,
    					   *ossia_tn0,
    					   *ossia_tn1);
    */
    // Error checking if it did not go well ? Rollback ?
    // Else...
    constraint->setStartEvent(sev->id());
    constraint->setEndEvent(eev->id());

    constraint->setStartDate(sev->date());
    constraint->setDefaultDuration(eev->date() - sev->date());
    constraint->setHeightPercentage((sev->heightPercentage() + eev->heightPercentage()) / 2.);

    sev->addNextConstraint(newConstraintModelId);
    eev->addPreviousConstraint(newConstraintModelId);

    // From now on everything must be in a valid state.
    addConstraint(constraint);
    emit constraintCreated(constraint->id());
}

void
ScenarioModel::createConstraintAndEndEventFromEvent(id_type<EventModel> startEventId,
        TimeValue constraint_duration,
        double heightPos,
        id_type<ConstraintModel> newConstraintId,
        id_type<AbstractConstraintViewModel> newConstraintFullViewId,
        id_type<EventModel> newEventId)
{
    auto startEvent = this->event(startEventId);

    auto constraint = new ConstraintModel {newConstraintId,
                                           newConstraintFullViewId,
                                           this->event(startEventId)->heightPercentage(),
                                           this
                                          };
    auto event = new EventModel {newEventId,
                                 heightPos,
                                 this
                                };


    if(startEventId == m_startEventId)
    {
        constraint->setHeightPercentage(heightPos);
    }
    else
    {
        constraint->setHeightPercentage((heightPos + startEvent->heightPercentage()) / 2);
    }

    // TEMPORARY :
    constraint->setStartDate(this->event(startEventId)->date());
    constraint->setDefaultDuration(constraint_duration);
    event->setDate(constraint->startDate() + constraint->defaultDuration());

    //	auto ossia_tn0 = this->event(startEventId)->apiObject();
    //	auto ossia_tn1 = event->apiObject();
    //	auto ossia_tb = constraint->apiObject();

    //	m_scenario->addTimeBox(*ossia_tb,
    //						   *ossia_tn0,
    //						   *ossia_tn1);

    // Error checking if it did not go well ? Rollback ?
    // Else...
    constraint->setStartEvent(startEventId);
    constraint->setEndEvent(event->id());

    // From now on everything must be in a valid state.
    m_events.push_back(event);
    m_constraints.push_back(constraint);

    emit eventCreated(event->id());
    emit constraintCreated(constraint->id());

    // link constraint with event
    event->addPreviousConstraint(newConstraintId);
    this->event(startEventId)->addNextConstraint(newConstraintId);
}

void ScenarioModel::createTimeNode(id_type<TimeNodeModel> timeNodeId,
                                   id_type<EventModel> eventId)
{
    auto newEvent = event(eventId);

    auto timeNode = new TimeNodeModel {timeNodeId,
                                       newEvent->date(),
                                       this
                                      };
    timeNode->addEvent(eventId);
    timeNode->setY(newEvent->heightPercentage());

    // TODO jm : TimeNode::addEvent devrait faire ça

    newEvent->changeTimeNode(timeNodeId);

    addTimeNode(timeNode);
}



///////// DELETION //////////
#include <tools/utilsCPP11.hpp>
void ScenarioModel::removeConstraint(id_type<ConstraintModel> constraintId)
{
    auto cstr = constraint(constraintId);
    vec_erase_remove_if(m_constraints,
                        [&constraintId](ConstraintModel * model)
    {
        return model->id() == constraintId;
    });

    auto sev = event(cstr->startEvent());
    sev->removeNextConstraint(constraintId);

    auto eev = event(cstr->endEvent());
    eev->removePreviousConstraint(constraintId);

    emit constraintRemoved(constraintId);

    delete cstr;
}

void ScenarioModel::removeEvent(id_type<EventModel> eventId)
{
    auto ev = event(eventId);

    for(auto constraint : ev->previousConstraints())
    {
        removeConstraint(constraint);
    }

    for(auto constraint : ev->nextConstraints())
    {
        removeConstraint(constraint);
    }

    vec_erase_remove_if(m_events,
                        [&eventId](EventModel * model)
    {
        return model->id() == eventId;
    });

    removeEventFromTimeNode(eventId);
    emit eventRemoved(eventId);
    delete ev;
}

void ScenarioModel::removeEventFromTimeNode(id_type<EventModel> eventId)
{
    for(auto& timeNode : m_timeNodes)
    {
        if(timeNode->removeEvent(eventId))
        {
            if(timeNode->isEmpty())
            {
                removeTimeNode(timeNode->id());
            }

            return;
        }
    }
}

void ScenarioModel::removeTimeNode(id_type<TimeNodeModel> timeNodeId)
{
    auto tn = timeNode(timeNodeId);
    vec_erase_remove_if(m_timeNodes,
                        [&timeNodeId](TimeNodeModel * model)
    {
        return model->id() == timeNodeId;
    });

    emit timeNodeRemoved(timeNodeId);
    delete tn;
}

void ScenarioModel::undo_removeConstraint(ConstraintModel* newConstraint)
{
    addConstraint(newConstraint);

    EventModel* sev = event(newConstraint->startEvent());
    EventModel* eev = event(newConstraint->endEvent());

    sev->addNextConstraint(newConstraint->id());
    eev->addPreviousConstraint(newConstraint->id());
}

void ScenarioModel::undo_createConstraintAndEndEventFromEvent(id_type<EventModel> endEventId)
{
    removeEvent(endEventId);
}

void ScenarioModel::undo_createConstraintBetweenEvent(id_type<ConstraintModel> constraintId)
{
    removeConstraint(constraintId);
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

std::vector<EventModel*> ScenarioModel::events() const
{
    return m_events;
}

void ScenarioModel::on_viewModelDestroyed(QObject* obj)
{
    removeViewModel(static_cast<ProcessViewModelInterface*>(obj));
}



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
