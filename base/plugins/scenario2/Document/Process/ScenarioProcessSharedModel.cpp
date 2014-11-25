#include "ScenarioProcessSharedModel.hpp"
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalModel.hpp>

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	iscore::ProcessSharedModelInterface{parent, "ScenarioProcessSharedModel", id}
{
	m_events.push_back(new EventModel(this, "EventModel", 1));
	m_events.push_back(new EventModel(this, "EventModel", 2));
	
}

iscore::ProcessViewModelInterface*ScenarioProcessSharedModel::makeViewModel(int id, QObject* parent)
{
}

int ScenarioProcessSharedModel::createIntervalAndBothEvents(int start, int dur)
{
	//1. Créer event e0 à start
	//2. Créer relation i0 startEvent - start
	/// Ajouter i0 à scenarioAPI
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
	
}

void ScenarioProcessSharedModel::deleteEvent(int eventId)
{
	
}