#include "ScenarioProcessSharedModel.hpp"

#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>

QDataStream& operator <<(QDataStream& s, const ScenarioProcessSharedModel& scenario)
{
	// Constraints
	s << (int) scenario.m_constraints.size();
	for(const auto& constraint : scenario.m_constraints)
	{
		s << *constraint;
	}

	// Events
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
	/*for(ConstraintModel* constraint : scenario.m_constraints)
	{
		auto sev = scenario.event(constraint->startEvent());
		auto eev = scenario.event(constraint->endEvent());

		scenario.m_scenario->addTimeBox(*constraint->apiObject(),
										*sev->apiObject(),
										*eev->apiObject());
	}*/

	return s;
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	ProcessSharedModelInterface{id, "ScenarioProcessSharedModel", parent},
	m_scenario{new OSSIA::Scenario},
	m_startEventId{0} // Always
{
	m_events.push_back(new EventModel{m_startEventId, this});
	//TODO demander à Clément si l'élément de fin sert vraiment à qqch ?
	//m_events.push_back(new EventModel(1, this));
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(QDataStream& s,
													   QObject* parent):
	ProcessSharedModelInterface{s, parent}
{
	s >> *this;
}

ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int viewModelId,
																	 QObject* parent)
{
	//TODO use a template here.
	auto scen = new TemporalScenarioProcessViewModel(viewModelId, this, parent);
	makeViewModel_impl(scen);
	return scen;
}

ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(QDataStream& s,
																	 QObject* parent)
{
	auto scen = new TemporalScenarioProcessViewModel(s, this, parent);
	makeViewModel_impl(scen);
	return scen;
}

void ScenarioProcessSharedModel::makeViewModel_impl(ScenarioProcessSharedModel::view_model_type* scen)
{
	addViewModel(scen);

	connect(scen, &TemporalScenarioProcessViewModel::destroyed,
			[this] (QObject* obj) { this->removeViewModel(static_cast<ProcessViewModelInterface*>(obj)); });

	connect(this, &ScenarioProcessSharedModel::constraintRemoved,
			scen, &view_model_type::on_constraintRemoved);

	connect(this, &ScenarioProcessSharedModel::eventCreated,
			scen, &view_model_type::eventCreated);
	connect(this, &ScenarioProcessSharedModel::eventDeleted,
			scen, &view_model_type::eventDeleted);
	connect(this, &ScenarioProcessSharedModel::eventMoved,
			scen, &view_model_type::eventMoved);
	connect(this, &ScenarioProcessSharedModel::constraintMoved,
			scen, &view_model_type::constraintMoved);
}


//////// Creation ////////
void ScenarioProcessSharedModel::createConstraintBetweenEvents(int startEventId, int endEventId, int newConstraintModelId)
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
}

void
ScenarioProcessSharedModel::createConstraintAndEndEventFromEvent(int startEventId,
																 int constraint_duration,
																 double heightPos,
																 int newConstraintId,
																 int newEventId)
{
	auto inter = new ConstraintModel{newConstraintId, this->event(startEventId)->heightPercentage(), this};
	auto event = new EventModel{newEventId, this->event(startEventId)->heightPercentage(), this};

	event->setHeightPercentage(heightPos);
	inter->setHeightPercentage(heightPos);

	// TEMPORARY :
	inter->setStartDate(this->event(startEventId)->date());
	inter->setWidth(constraint_duration);
	event->setDate(inter->startDate() + inter->width());
	//	event->m_y = inter->heightPercentage() * 75;

	auto ossia_tn0 = this->event(startEventId)->apiObject();
	auto ossia_tn1 = event->apiObject();
	auto ossia_tb = inter->apiObject();

	m_scenario->addTimeBox(*ossia_tb,
						   *ossia_tn0,
						   *ossia_tn1);

	// Error checking if it did not go well ? Rollback ?
	// Else...
	inter->setStartEvent(startEventId);
	inter->setEndEvent(event->id());

	// From now on everything must be in a valid state.
	m_events.push_back(event);
	m_constraints.push_back(inter);

	emit eventCreated(event->id());
	emit constraintCreated(inter->id());

	// link constraint with event
	event->addPreviousConstraint(newConstraintId);
	this->event(startEventId)->addNextConstraint(newConstraintId);
	event->setVerticalExtremity(inter->id(), inter->heightPercentage());
	this->event(startEventId)->setVerticalExtremity(inter->id(), inter->heightPercentage());
}


void ScenarioProcessSharedModel::moveEventAndConstraint(int eventId, int absolute_time, double heightPosition)
{
	// resize previous constraint
	if (! event(eventId)->previousConstraints().isEmpty() )
	{
		auto ev = event(eventId);

		int time = absolute_time - ev->date();

		ev->setHeightPercentage(heightPosition);
		ev->translate(time);

		auto prev_constraint = constraint(event(eventId)->previousConstraints().at(0));
		prev_constraint->setWidth(prev_constraint->width() + time);
		emit constraintMoved(prev_constraint->id());
		emit eventMoved(eventId);

		QVector<int> already_moved_events;
		moveNextElements(eventId, time, already_moved_events);
	}
}

void ScenarioProcessSharedModel::moveConstraint(int constraintId, int absolute_time, double heightPosition)
{
	constraint(constraintId)->setHeightPercentage(heightPosition);
	emit constraintMoved(constraintId);

	// redraw constraint-event link
	auto eev = event(constraint(constraintId)->endEvent());
	auto sev = event(constraint(constraintId)->startEvent());
	eev->setVerticalExtremity(constraintId, heightPosition);
	sev->setVerticalExtremity(constraintId, heightPosition);

	moveEventAndConstraint(sev->id(), absolute_time, sev->heightPercentage());
}

void ScenarioProcessSharedModel::moveNextElements(int firstEventMovedId, int deltaTime, QVector<int> &movedEvent)
{
	auto cur_event = event(firstEventMovedId);

	if (! cur_event->previousConstraints().isEmpty())
	{
		for (auto cons : cur_event->nextConstraints())
		{
			auto evId = constraint(cons)->endEvent();
			if (movedEvent.indexOf(evId) == -1)
			{
				event(evId)->translate(deltaTime);
				movedEvent.push_back(evId);
				constraint(cons)->translate(deltaTime);
				emit eventMoved(evId);
				emit constraintMoved(cons);
				moveNextElements(evId, deltaTime, movedEvent);
			}
		}
	}
}

///////// DELETION //////////
#include <tools/utilsCPP11.hpp>
void ScenarioProcessSharedModel::undo_createConstraintBetweenEvents(int constraintId)
{
	emit constraintRemoved(constraintId);
	removeById(m_constraints, constraintId);
}

void ScenarioProcessSharedModel::undo_createConstraintAndEndEventFromEvent(int constraintId)
{
	// End event suppression
	{
		auto end_event_id = this->constraint(constraintId)->endEvent();

		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);

		auto start_event = event(constraint(constraintId)->startEvent());
		start_event->removeNextConstraint(constraintId);
	}

	// Constraint suppression
	{
		emit constraintRemoved(constraintId);
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
		emit constraintRemoved(constraintId);
		removeById(m_constraints, constraintId);
	}

	// Mid event suppression
	{
		emit eventDeleted(mid_event->id());
		removeById(m_events, mid_event->id());
	}

	// First constraint suppression
	{
		auto first_event = event(constraint(start_constraint_id)->startEvent());
		emit constraintRemoved(start_constraint->id());
		first_event->removeNextConstraint(start_constraint_id);
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

void ScenarioProcessSharedModel::serialize(QDataStream& s) const
{
	s << *this;
}

void ScenarioProcessSharedModel::deserialize(QDataStream& s)
{
	s >> *this;
}
