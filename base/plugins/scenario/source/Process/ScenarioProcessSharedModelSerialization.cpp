#include "ScenarioProcessSharedModelSerialization.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModelSerialization.hpp"
#include "Document/Constraint/ConstraintModelSerialization.hpp"
#include "ScenarioProcessSharedModel.hpp"

template<>
void Visitor<Reader<DataStream>>::visit(const ScenarioProcessSharedModel& scenario)
{
	// Constraints
	m_stream << (int) scenario.m_constraints.size();
	for(const ConstraintModel* constraint : scenario.m_constraints)
	{
		visit(*constraint);
	}

	// Events
	m_stream << (int) scenario.m_events.size();
	for(const EventModel* event : scenario.m_events)
	{
		visit(*event);
	}
}

template<>
void Visitor<Writer<DataStream>>::visit<ScenarioProcessSharedModel>(ScenarioProcessSharedModel& scenario)
{
	// Constraints
	int constraint_count;
	m_stream >> constraint_count;
	qDebug() << constraint_count;
	for(; constraint_count --> 0;)
	{
		qDebug() << constraint_count;
		ConstraintModel* constraint = new ConstraintModel{*this, &scenario};
		scenario.addConstraint(constraint);
	}

	// Events
	int event_count;
	m_stream >> event_count;
	qDebug() << event_count;
	for(; event_count --> 0;)
	{
		EventModel* evmodel = new EventModel(*this, &scenario);
		scenario.addEvent(evmodel);
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
}

