#include "ScenarioProcessSharedModel.hpp"
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalModel.hpp>

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>

// Note : creating an event implies the creation of a 

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	iscore::ProcessSharedModelInterface{parent, "ScenarioProcessSharedModel", id}, 
	m_scenario{new OSSIA::Scenario}
{
	m_events.push_back(new EventModel(0, this));
	m_events.push_back(new EventModel(1, this));
	
}

iscore::ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int id, QObject* parent)
{
}

int ScenarioProcessSharedModel::createIntervalAndBothEvents(int start, int dur)
{
	//1. Créer event e0 à start. 
	/// -> crée la relation i0 = startEvent - start
	/// -> Ajoute i0 à scenarioAPI (il faut le récupérer ensuite).
	auto ev0_id = createEvent(start);
	EventModel* ev0 = event(ev0_id);
	
	//3. Créer event e1 à dur
	//4. Créer relation i1 à e0 - e1
	/// Ajouter i1 à scenarioAPI
	//5. Renvoyer id de i1
	
	auto interval = new IntervalModel(m_nextIntervalId++, 
									  this);
	m_intervals.push_back(interval);

	// TODO emit
}

int ScenarioProcessSharedModel::createIntervalAndEndEvent(int startEventId, int dur)
{
	
}

int ScenarioProcessSharedModel::createIntervalBetweenEvents(int startEventId, int endEventId)
{
	
}

void ScenarioProcessSharedModel::deleteInterval(int intervalId)
{
	
}

void ScenarioProcessSharedModel::deleteIntervalAndLastEvent(int intervalId)
{
	
}

void ScenarioProcessSharedModel::deleteIntervalAndBothEvents(int intervalId)
{
	
}

IntervalModel* ScenarioProcessSharedModel::interval(int intervalId)
{
	return findById(m_intervals, intervalId);
}

int ScenarioProcessSharedModel::createEvent(int time)
{
	auto event = new EventModel{m_nextEventId++, this};
	auto inter = new IntervalModel{m_nextIntervalId++, this};
	
	auto ossia_tn0 = startEvent()->apiObject();
	auto ossia_tn1 = event->apiObject();
	auto ossia_tb = inter->apiObject();
	
	m_scenario->addTimeBox(*ossia_tb, 
						   *ossia_tn0,
						   *ossia_tn1);
	
	// Error checking if it did not go well ? Rollback ?
	// Else...
	
	emit eventCreated(event->id());
	emit intervalCreated(inter->id());
	
	return event->id();
}

void ScenarioProcessSharedModel::deleteEvent(int eventId)
{
	// What happens ??
}

EventModel* ScenarioProcessSharedModel::event(int eventId)
{
	
}