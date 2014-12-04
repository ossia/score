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
	m_events.push_back(new EventModel{getNextId(m_events), this});
	//TODO demander à Clément si l'élément de fin sert vraiment à qqch ?
	//m_events.push_back(new EventModel(1, this));
}

ScenarioProcessSharedModel::ScenarioProcessSharedModel(QDataStream& s,
													   QObject* parent):
	iscore::ProcessSharedModelInterface{nullptr, "ScenarioProcessSharedModel", -1}
{
	s >> static_cast<iscore::ProcessSharedModelInterface&>(*this);

	this->setParent(parent);
}

iscore::ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(int viewModelId,
																			 int processId,
																			 QObject* parent)
{
	auto scen = new ScenarioProcessViewModel(viewModelId, processId, parent);
	connect(this, &ScenarioProcessSharedModel::eventCreated,
			scen, &ScenarioProcessViewModel::eventCreated);
	connect(this, &ScenarioProcessSharedModel::eventDeleted,
			scen, &ScenarioProcessViewModel::eventDeleted);
	connect(this, &ScenarioProcessSharedModel::intervalCreated,
			scen, &ScenarioProcessViewModel::intervalCreated);
	connect(this, &ScenarioProcessSharedModel::intervalDeleted,
			scen, &ScenarioProcessViewModel::intervalDeleted);

	return scen;
}

iscore::ProcessViewModelInterface*ScenarioProcessSharedModel::makeViewModel(QDataStream& s, QObject* parent)
{
	return new ScenarioProcessViewModel(s, parent);
}

void ScenarioProcessSharedModel::serialize(QDataStream& s) const
{
	s << (int) m_intervals.size();
	for(auto& interval : m_intervals)
	{
		s << *interval;
	}

	s << (int) m_events.size();
	for(auto& event : m_events)
	{
		s << *event;
	}
}

void ScenarioProcessSharedModel::deserialize(QDataStream& s)
{
	// Intervals
	int interval_count;
	s >> interval_count;
	for(int i = 0; i < interval_count; i++)
	{
		IntervalModel* interval = new IntervalModel(s, this);
		m_intervals.push_back(interval);
		emit intervalCreated(interval->id());
	}

	// Events
	int event_count;
	s >> event_count;
	for(int i = 0; i < event_count; i++)
	{
		EventModel* evmodel = new EventModel(s, this);
		m_events.push_back(evmodel);

		emit eventCreated(evmodel->id());
	}

	for(IntervalModel* interval : m_intervals)
	{
		auto sev = event(interval->startEvent());
		auto eev = event(interval->endEvent());

		m_scenario->addTimeBox(*interval->apiObject(),
							   *sev->apiObject(),
							   *eev->apiObject());
	}
}

//////// Creation ////////
int ScenarioProcessSharedModel::createIntervalBetweenEvents(int startEventId, int endEventId)
{
	auto sev = this->event(startEventId);
	auto eev = this->event(endEventId);
	auto inter = new IntervalModel{getNextId(m_intervals), this};

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

	// From now on everything must be in a valid state.
	m_intervals.push_back(inter);

	emit intervalCreated(inter->id());

	return inter->id();
}

std::tuple<int, int>
ScenarioProcessSharedModel::createIntervalAndEndEventFromEvent(int startEventId,
                                                               int interval_duration,
                                                               double heightPos)
{
    auto event = new EventModel{getNextId(m_events), this->event(startEventId)->heightPercentage(), this};
    auto inter = new IntervalModel{getNextId(m_intervals), this->event(startEventId)->heightPercentage(), this};


    if (startEventId == startEvent()->id()) {
        event->setHeightPercentage(heightPos);
        inter->setHeightPercentage(heightPos);
    }

	// TEMPORARY :
	inter->m_x = this->event(startEventId)->m_x;
	inter->m_width = interval_duration;
	event->m_x = inter->m_x + inter->m_width;
//	event->m_y = inter->heightPercentage() * 75;


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
	m_intervals.push_back(inter);

	emit eventCreated(event->id());
	emit intervalCreated(inter->id());

	return std::make_tuple(inter->id(), event->id());
}


std::tuple<int, int, int, int> ScenarioProcessSharedModel::createIntervalAndBothEvents(int start, int dur, double heightPos)
{
    auto t1 = createIntervalAndEndEventFromStartEvent(start, heightPos);
    auto t2 = createIntervalAndEndEventFromEvent(std::get<1>(t1), dur, heightPos);

	return std::tuple_cat(t1, t2);
}

std::tuple<int, int> ScenarioProcessSharedModel::createIntervalAndEndEventFromStartEvent(int endTime, double heightPos)
{
    return createIntervalAndEndEventFromEvent(startEvent()->id(), endTime, heightPos);
}

///////// DELETION //////////
#include <utilsCPP11.hpp>
void ScenarioProcessSharedModel::undo_createIntervalBetweenEvents(int intervalId)
{
	emit intervalDeleted(intervalId);
	removeById(m_intervals, intervalId);
}

void ScenarioProcessSharedModel::undo_createIntervalAndEndEventFromEvent(int intervalId)
{
	// End event suppression
	{
		qDebug() << "suppressing event";
		auto end_event_id = this->interval(intervalId)->endEvent();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
	}

	// Interval suppression
	{
		qDebug() << "suppressing interval";
		emit intervalDeleted(intervalId);
		removeById(m_intervals, intervalId);
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
		auto end_event_id = this->interval(intervalId)->endEvent();
		emit eventDeleted(end_event_id);
		removeById(m_events, end_event_id);
	}

	// Get the mid event id before deletion of the interval
	auto mid_event_id = this->interval(intervalId)->startEvent();
	auto mid_event = event(mid_event_id);
	auto start_interval_id = mid_event->previousIntervals().front();
	auto start_interval = interval(start_interval_id);

	// Interval suppression
	{
		emit intervalDeleted(intervalId);
		removeById(m_intervals, intervalId);
	}

	// Mid event suppression
	{
		emit eventDeleted(mid_event->id());
		removeById(m_events, mid_event->id());
	}

	// First interval suppression
	{
		emit intervalDeleted(start_interval->id());
		removeById(m_intervals, start_interval->id());
	}
}






/////////////////////////////
IntervalModel* ScenarioProcessSharedModel::interval(int intervalId)
{
	return findById(m_intervals, intervalId);
}

EventModel* ScenarioProcessSharedModel::event(int eventId)
{
	return findById(m_events, eventId);
}
