#include "ScenarioProcessSharedModel.hpp"
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalModel.hpp>

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	iscore::ProcessSharedModelInterface{parent, "ScenarioProcessSharedModel", id},
	m_startEvent{new EventModel(this, "EventModel", -2)},
	m_endEvent{new EventModel(this, "EventModel", -1)}
  
{
	
}

iscore::ProcessViewModelInterface*ScenarioProcessSharedModel::makeViewModel(int id, QObject* parent)
{
}

void ScenarioProcessSharedModel::createInterval(int dur)
{
	auto interval = new IntervalModel(m_startEvent, nullptr, m_nextIntervalId++, this);
	m_intervals.push_back(interval);
	
	// TODO emit
}

void ScenarioProcessSharedModel::createInterval(EventModel* start, int dur)
{
	
}

IntervalModel* ScenarioProcessSharedModel::interval(int intervalId)
{
	return findById(m_intervals, intervalId);
}