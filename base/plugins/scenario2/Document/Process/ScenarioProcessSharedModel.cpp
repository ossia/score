#include "ScenarioProcessSharedModel.hpp"
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalModel.hpp>

#include <API/Headers/Editor/Scenario.h>

#include <QDebug>
#include "ScenarioProcessViewModel.hpp"

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QObject* parent):
	iscore::ProcessSharedModelInterface{parent, "ScenarioProcessSharedModel", id}, 
	m_scenario{new OSSIA::Scenario}
{
	m_events.push_back(new EventModel(0, this));
	m_events.push_back(new EventModel(1, this));
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(int id, QByteArray data, QObject* parent):
	iscore::ProcessSharedModelInterface{parent, "ScenarioProcessSharedModel", id}
{
	qDebug() << "TODO =====" << Q_FUNC_INFO;
}

iscore::ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int id, QObject* parent)
{
	qDebug(Q_FUNC_INFO);
	return new ScenarioProcessViewModel(id, parent);
}

//////// Creation ////////
int ScenarioProcessSharedModel::createIntervalBetweenEvents(int startEventId, int endEventId)
{
	auto sev = this->event(startEventId);
	auto eev = this->event(endEventId);
	auto inter = new IntervalModel{m_nextIntervalId++, this};
	
	auto ossia_tn0 = sev->apiObject();
	auto ossia_tn1 = eev->apiObject();
	auto ossia_tb = inter->apiObject();
	
	m_scenario->addTimeBox(*ossia_tb, 
						   *ossia_tn0,
						   *ossia_tn1);
	
	// Error checking if it did not go well ? Rollback ?
	// Else...
	inter->setStartEvent(sev);
	inter->setEndEvent(eev);
	
	// From now on everything must be in a valid state.
	m_intervals.push_back(inter);
	
	emit intervalCreated(inter->id());
	
	return inter->id();
}

std::tuple<int, int> ScenarioProcessSharedModel::createIntervalAndEndEventFromEvent(int startEventId, int dur)
{
	auto event = new EventModel{m_nextEventId++, this};
	auto inter = new IntervalModel{m_nextIntervalId++, this};
	
	auto ossia_tn0 = this->event(startEventId)->apiObject();
	auto ossia_tn1 = event->apiObject();
	auto ossia_tb = inter->apiObject();
	
	m_scenario->addTimeBox(*ossia_tb, 
						   *ossia_tn0,
						   *ossia_tn1);
	
	// Error checking if it did not go well ? Rollback ?
	// Else...
	inter->setStartEvent(startEvent());
	inter->setEndEvent(event);
	
	// From now on everything must be in a valid state.
	m_events.push_back(event);
	m_intervals.push_back(inter);
	
	emit eventCreated(event->id());
	emit intervalCreated(inter->id());
	
	return std::make_tuple(inter->id(), event->id());
}


std::tuple<int, int, int, int> ScenarioProcessSharedModel::createIntervalAndBothEvents(int start, int dur)
{
	auto t1 = createIntervalAndEndEventFromStartEvent(start);
	auto t2 = createIntervalAndEndEventFromEvent(std::get<1>(t1), dur);
	
	return std::tuple_cat(t1, t2);
}

std::tuple<int, int> ScenarioProcessSharedModel::createIntervalAndEndEventFromStartEvent(int endTime)
{
	return createIntervalAndEndEventFromEvent(startEvent()->id(), endTime);
}

///////// DELETION //////////
#include <utilsCPP11.hpp>
void ScenarioProcessSharedModel::undo_createIntervalBetweenEvents(int intervalId)
{
	emit intervalDeleted(intervalId);
	removeById(m_intervals, intervalId);
	
	m_nextIntervalId--;
}

void ScenarioProcessSharedModel::undo_createIntervalAndEndEventFromEvent(int intervalId)
{
	// End event suppression
	{ 
		auto end_event_id = this->interval(intervalId)->endEvent()->id();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
		m_nextEventId--;
	}
	
	// Interval suppression
	{
		emit intervalDeleted(intervalId);
		removeById(m_intervals, intervalId);
		m_nextIntervalId--;
	}
}

void ScenarioProcessSharedModel::undo_createIntervalAndEndEventFromStartEvent(int intervalId)
{
	undo_createIntervalAndEndEventFromEvent(intervalId);
}

void ScenarioProcessSharedModel::undo_createIntervalAndBothEvents(int intervalId)
{
	// End event suppression
	{ 
		auto end_event_id = this->interval(intervalId)->endEvent()->id();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
		m_nextEventId--;
	}
	
	// Get the mid event id before deletion of the interval
	auto mid_event = this->interval(intervalId)->startEvent();
	auto start_interval = mid_event->previousIntervals().front();
	
	// Interval suppression
	{
		emit intervalDeleted(intervalId);
		removeById(m_intervals, intervalId);
		m_nextIntervalId--;
	}
	
	// Mid event suppression
	{
		emit eventDeleted(mid_event->id());
		removeById(m_events, mid_event->id());
		m_nextEventId--;
	}
	
	// First interval suppression
	{
		emit intervalDeleted(start_interval->id());
		removeById(m_intervals, start_interval->id());
		m_nextIntervalId--;
	}	
}






/////////////////////////////
IntervalModel* ScenarioProcessSharedModel::interval(int intervalId)
{
	return findById(m_intervals, intervalId);
}

EventModel* ScenarioProcessSharedModel::event(int eventId)
{
	
}