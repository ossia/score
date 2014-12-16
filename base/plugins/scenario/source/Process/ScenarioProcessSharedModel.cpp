#include "ScenarioProcessSharedModel.hpp"

#include "Process/ScenarioProcessViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>

QDataStream& operator <<(QDataStream& s, const ScenarioProcessSharedModel& scenario)
{
	s << (int) scenario.m_constraints.size();
	for(const auto& constraint : scenario.m_constraints)
	{
		s << *constraint;
	}

	s << (int) scenario.m_events.size();
	for(const auto& event : scenario.m_events)
	{
		s << *event;
	}

	return s;
}

QDataStream& operator >>(QDataStream& s, ScenarioProcessSharedModel& scenario)
{
	// Constraints
	int constraint_count;
	s >> constraint_count;
	for(; constraint_count --> 0;)
	{
		ConstraintModel* constraint = new ConstraintModel{s, &scenario};
		scenario.m_constraints.push_back(constraint);
		emit scenario.constraintCreated(constraint->id());
	}

	// Events
	int event_count;
	s >> event_count;
	for(; event_count --> 0;)
	{
		EventModel* evmodel = new EventModel(s, &scenario);
		scenario.m_events.push_back(evmodel);

		emit scenario.eventCreated(evmodel->id());
	}

	// Recreate the API
	for(ConstraintModel* constraint : scenario.m_constraints)
	{
		auto sev = scenario.event(constraint->startEvent());
		auto eev = scenario.event(constraint->endEvent());

		scenario.m_scenario->addTimeBox(*constraint->apiObject(),
										*sev->apiObject(),
										*eev->apiObject());
	}

	return s;
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	ProcessSharedModelInterface{id, "ScenarioProcessSharedModel", parent},
	m_scenario{new OSSIA::Scenario},
	m_startEventId{getNextId()}
{
	m_events.push_back(new EventModel{m_startEventId, this});
	//TODO demander à Clément si l'élément de fin sert vraiment à qqch ?
	//m_events.push_back(new EventModel(1, this));
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(QDataStream& s,
													   QObject* parent):
	ProcessSharedModelInterface{s, "ScenarioProcessSharedModel", parent}
{
	s >> *this;
}

ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int viewModelId,
																	 int processId,
																	 QObject* parent)
{
	auto scen = new ScenarioProcessViewModel(viewModelId, processId, parent);
	connect(this, &ScenarioProcessSharedModel::eventCreated,
			scen, &ScenarioProcessViewModel::eventCreated);
	connect(this, &ScenarioProcessSharedModel::eventDeleted,
			scen, &ScenarioProcessViewModel::eventDeleted);
	connect(this, &ScenarioProcessSharedModel::constraintCreated,
			scen, &ScenarioProcessViewModel::constraintCreated);
	connect(this, &ScenarioProcessSharedModel::constraintDeleted,
			scen, &ScenarioProcessViewModel::constraintDeleted);
	connect(this, &ScenarioProcessSharedModel::eventMoved,
			scen, &ScenarioProcessViewModel::eventMoved);
	connect(this, &ScenarioProcessSharedModel::constraintMoved,
			scen, &ScenarioProcessViewModel::constraintMoved);

	return scen;
}

ProcessViewModelInterface*ScenarioProcessSharedModel::makeViewModel(QDataStream& s, QObject* parent)
{
	return new ScenarioProcessViewModel(s, parent);
}


//////// Creation ////////
int ScenarioProcessSharedModel::createConstraintBetweenEvents(int startEventId, int endEventId, int newConstraintModelId)
{
	auto sev = this->event(startEventId);
	auto eev = this->event(endEventId);
	auto inter = new ConstraintModel{newConstraintModelId, this};

	auto ossia_tn0 = sev->apiObject();
	auto ossia_tn1 = eev->apiObject();
	auto ossia_tb = inter->apiObject();

	m_scenario->addTimeBox(*ossia_tb,
						   *ossia_tn0,
						   *ossia_tn1);

	// Error checking if it did not go well ? Rollback ?
	// Else...
	inter->setStartEvent(sev->id());
	inter->setEndEvent(eev->id());

    sev->addNextConstraint(newConstraintModelId);
    eev->addPreviousConstraint(newConstraintModelId);

	// From now on everything must be in a valid state.

	emit constraintCreated(inter->id());

	return inter->id();
}

std::tuple<int, int>
ScenarioProcessSharedModel::createConstraintAndEndEventFromEvent(int startEventId,
															   int constraint_duration,
															   double heightPos,
															   int newConstraintId,
															   int newEventId)
{
	auto inter = new ConstraintModel{newConstraintId, this->event(startEventId)->heightPercentage(), this};
	auto event = new EventModel{newEventId, this->event(startEventId)->heightPercentage(), this};


	if (startEventId == startEvent()->id()) {
		event->setHeightPercentage(heightPos);
		inter->setHeightPercentage(heightPos);
	}

	// TEMPORARY :
	inter->m_x = this->event(startEventId)->m_x;
	inter->m_width = constraint_duration;
	event->m_x = inter->m_x + inter->m_width;
//	event->m_y = inter->heightPercentage() * 75;

    event->addPreviousConstraint(newConstraintId);
    this->event(startEventId)->addNextConstraint(newConstraintId);

//    event->setVerticalExtremity(inter->heightPercentage());
//    qDebug() << "create constraint " << inter->heightPercentage() ;

	auto ossia_tn0 = this->event(startEventId)->apiObject();
	auto ossia_tn1 = event->apiObject();
	auto ossia_tb = inter->apiObject();

	m_scenario->addTimeBox(*ossia_tb,
						   *ossia_tn0,
						   *ossia_tn1);

	// Error checking if it did not go well ? Rollback ?
	// Else...
	inter->setStartEvent(startEvent()->id());
	inter->setEndEvent(event->id());

	// From now on everything must be in a valid state.
	m_events.push_back(event);
	m_constraints.push_back(inter);

	emit eventCreated(event->id());
	emit constraintCreated(inter->id());

	return std::make_tuple(inter->id(), event->id());
}


std::tuple<int, int, int, int> ScenarioProcessSharedModel::createConstraintAndBothEvents(int start, int dur, double heightPos,
																					   int createdFirstConstraintId,
																					   int createdFirstEventId,
																					   int createdSecondConstraintId,
																					   int createdSecondEventId)
{
	auto t1 = createConstraintAndEndEventFromStartEvent(start,
													  heightPos,
													  createdFirstConstraintId,
													  createdFirstEventId);

	auto t2 = createConstraintAndEndEventFromEvent(createdFirstEventId,
												 dur,
												 heightPos,
												 createdSecondConstraintId,
												 createdSecondEventId);

	return std::tuple_cat(t1, t2);
}

std::tuple<int, int> ScenarioProcessSharedModel::createConstraintAndEndEventFromStartEvent(int endTime,
																						 double heightPos,
																						 int newConstraintId,
																						 int newEventId)
{
	return createConstraintAndEndEventFromEvent(startEvent()->id(),
											  endTime,
											  heightPos,
											  newConstraintId,
											  newEventId);
}

void ScenarioProcessSharedModel::moveEventAndConstraint(int eventId, int time, double heightPosition)
{
    event(eventId)->setHeightPercentage(heightPosition);
//    event(eventId)->setVerticalExtremity(heightPosition);
    emit eventMoved(eventId);
}

void ScenarioProcessSharedModel::moveConstraint(int constraintId, double heightPosition)
{
    constraint(constraintId)->setHeightPercentage(heightPosition);
    auto eev = event(constraint(constraintId)->endEvent());
    eev->setVerticalExtremity(heightPosition);

    emit constraintMoved(constraintId);
}

///////// DELETION //////////
#include <tools/utilsCPP11.hpp>
void ScenarioProcessSharedModel::undo_createConstraintBetweenEvents(int constraintId)
{
	emit constraintDeleted(constraintId);
	removeById(m_constraints, constraintId);
}

void ScenarioProcessSharedModel::undo_createConstraintAndEndEventFromEvent(int constraintId)
{
	// End event suppression
	{
		qDebug() << "suppressing event";
		auto end_event_id = this->constraint(constraintId)->endEvent();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
	}

	// Constraint suppression
	{
		qDebug() << "suppressing constraint";
		emit constraintDeleted(constraintId);
		removeById(m_constraints, constraintId);
	}
}

void ScenarioProcessSharedModel::undo_createConstraintAndEndEventFromStartEvent(int constraintId)
{
	undo_createConstraintAndEndEventFromEvent(constraintId);
}

void ScenarioProcessSharedModel::undo_createConstraintAndBothEvents(int constraintId)
{
	// End event suppression
	{
		auto end_event_id = this->constraint(constraintId)->endEvent();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
	}

	// Get the mid event id before deletion of the constraint
	auto mid_event_id = this->constraint(constraintId)->startEvent();
	auto mid_event = event(mid_event_id);
	auto start_constraint_id = mid_event->previousConstraints().front();
	auto start_constraint = constraint(start_constraint_id);

	// Constraint suppression
	{
		emit constraintDeleted(constraintId);
		removeById(m_constraints, constraintId);
	}

	// Mid event suppression
	{
		emit eventDeleted(mid_event->id());
		removeById(m_events, mid_event->id());
	}

	// First constraint suppression
	{
		emit constraintDeleted(start_constraint->id());
		removeById(m_constraints, start_constraint->id());
	}
}






/////////////////////////////
ConstraintModel* ScenarioProcessSharedModel::constraint(int constraintId)
{
	return findById(m_constraints, constraintId);
}

EventModel* ScenarioProcessSharedModel::event(int eventId)
{
	return findById(m_events, eventId);
}

EventModel* ScenarioProcessSharedModel::startEvent()
{
	return event(m_startEventId);
}

EventModel* ScenarioProcessSharedModel::endEvent()
{
	return event(m_endEventId);
}
