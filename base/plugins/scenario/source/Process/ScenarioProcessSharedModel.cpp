#include "ScenarioProcessSharedModel.hpp"

#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	ProcessSharedModelInterface{id, "ScenarioProcessSharedModel", parent},
	m_scenario{new OSSIA::Scenario},
	m_startEventId{0} // Always
{
	m_events.push_back(new EventModel{m_startEventId, this});
	//TODO demander à Clément si l'élément de fin sert vraiment à qqch ?
	//m_events.push_back(new EventModel(1, this));
}

ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int viewModelId,
																	 QObject* parent)
{
	//TODO additional view model types.
	auto scen = new TemporalScenarioProcessViewModel(viewModelId, this, parent);
	makeViewModel_impl(scen);
	return scen;
}


#include <interface/serialization/DataStreamVisitor.hpp>
ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(SerializationIdentifier identifier,
																	 void* data,
																	 QObject* parent)
{
	if(identifier == DataStream::type())
	{
		auto scen = new TemporalScenarioProcessViewModel(*static_cast<Deserializer<DataStream>*>(data),
														 this,
														 parent);
		makeViewModel_impl(scen);
		return scen;
	}

	throw std::runtime_error("ScenarioProcessViewModels only supports DataStream serialization");

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
	connect(this, &ScenarioProcessSharedModel::eventRemoved,
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
	inter->setStartEvent((SettableIdentifier::identifier_type) sev->id());
	inter->setEndEvent((SettableIdentifier::identifier_type) eev->id());

	sev->addNextConstraint(newConstraintModelId);
	eev->addPreviousConstraint(newConstraintModelId);

	// From now on everything must be in a valid state.

	emit constraintCreated((SettableIdentifier::identifier_type) inter->id());
}

void
ScenarioProcessSharedModel::createConstraintAndEndEventFromEvent(int startEventId,
																 int constraint_duration,
																 double heightPos,
																 int newConstraintId,
																 int newEventId)
{
	auto constraint = new ConstraintModel{newConstraintId,
					  this->event(startEventId)->heightPercentage(),
					  this};
	auto event = new EventModel{newEventId,
				 this->event(startEventId)->heightPercentage(),
				 this};

	event->setHeightPercentage(heightPos);
	constraint->setHeightPercentage(heightPos);

	// TEMPORARY :
	constraint->setStartDate(this->event(startEventId)->date());
	constraint->setWidth(constraint_duration);
	event->setDate(constraint->startDate() + constraint->width());
	//	event->m_y = inter->heightPercentage() * 75;

	auto ossia_tn0 = this->event(startEventId)->apiObject();
	auto ossia_tn1 = event->apiObject();
	auto ossia_tb = constraint->apiObject();

	m_scenario->addTimeBox(*ossia_tb,
						   *ossia_tn0,
						   *ossia_tn1);

	// Error checking if it did not go well ? Rollback ?
	// Else...
	constraint->setStartEvent(startEventId);
	constraint->setEndEvent((SettableIdentifier::identifier_type) event->id());

	// From now on everything must be in a valid state.
	m_events.push_back(event);
	m_constraints.push_back(constraint);

	emit eventCreated((SettableIdentifier::identifier_type) event->id());
	emit constraintCreated((SettableIdentifier::identifier_type) constraint->id());

	// link constraint with event
	event->addPreviousConstraint(newConstraintId);
	this->event(startEventId)->addNextConstraint(newConstraintId);
	event->setVerticalExtremity((SettableIdentifier::identifier_type) constraint->id(),
								constraint->heightPercentage());
	this->event(startEventId)->setVerticalExtremity((SettableIdentifier::identifier_type) constraint->id(),
													constraint->heightPercentage());
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
		emit constraintMoved((SettableIdentifier::identifier_type) prev_constraint->id());
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

	moveEventAndConstraint((SettableIdentifier::identifier_type) sev->id(),
						   absolute_time,
						   sev->heightPercentage());
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
void ScenarioProcessSharedModel::removeConstraint(int constraintId)
{
	auto cstr = constraint(constraintId);
	vec_erase_remove_if(m_constraints,
						[&constraintId] (ConstraintModel* model)
						{ return model->id() == constraintId; });

	emit constraintRemoved(constraintId);
	delete cstr;
}

void ScenarioProcessSharedModel::removeEvent(int eventId)
{
	auto ev = event(eventId);
	vec_erase_remove_if(m_events,
						[&eventId] (EventModel* model)
						{ return model->id() == eventId; });

	emit eventRemoved(eventId);
	delete ev;
}

void ScenarioProcessSharedModel::undo_createConstraintAndEndEventFromEvent(int constraintId)
{
	// End event suppression
	{
		auto end_event_id = this->constraint(constraintId)->endEvent();

		emit eventRemoved(end_event_id);
		removeById(m_events, end_event_id);

		auto start_event = event(constraint(constraintId)->startEvent());
		start_event->removeNextConstraint(constraintId);
	}

	// Constraint suppression
	removeConstraint(constraintId);
}


/////////////////////////////
ConstraintModel* ScenarioProcessSharedModel::constraint(int constraintId) const
{
	return findById(m_constraints, constraintId);
}

EventModel* ScenarioProcessSharedModel::event(int eventId) const
{
	return findById(m_events, eventId);
}

EventModel* ScenarioProcessSharedModel::startEvent() const
{
	return event(m_startEventId);
}

EventModel* ScenarioProcessSharedModel::endEvent() const
{
	return event(m_endEventId);
}



void ScenarioProcessSharedModel::addConstraint(ConstraintModel* constraint)
{
	m_constraints.push_back(constraint);
	emit constraintCreated((SettableIdentifier::identifier_type) constraint->id());
}

void ScenarioProcessSharedModel::addEvent(EventModel* event)
{
	m_events.push_back(event);
	emit eventCreated((SettableIdentifier::identifier_type) event->id());
}
