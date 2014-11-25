#pragma once
#include <interface/process/ProcessSharedModelInterface.hpp>

class IntervalModel;
class EventModel;
class ScenarioProcessSharedModel : public iscore::ProcessSharedModelInterface
{
	Q_OBJECT
	
	public:
		ScenarioProcessSharedModel(int id, QObject* parent);
		virtual ~ScenarioProcessSharedModel() = default;
		virtual iscore::ProcessViewModelInterface* makeViewModel(int id, QObject* parent);
		
		virtual QByteArray serialize() override { return {}; }
		virtual void deserialize(QByteArray&&) override { }
		
		// Intervals
		int createIntervalAndBothEvents(int startTime, int duration);
		int createIntervalAndEndEvent(int startEventId, int duration);
		int createIntervalBetweenEvents(int startEventId, int endEventId);
		
		void deleteInterval(int intervalId);
		void deleteIntervalAndLastEvent(int intervalId);
		void deleteIntervalAndBothEvents(int intervalId);
		
		IntervalModel* interval(int intervalId);
		
		// Events
		int createEvent(int time); // Creates an interval btween the start event and this one
		void deleteEvent(int eventId); // ?
		
	private:
		std::vector<IntervalModel*> m_intervals;
		std::vector<EventModel*> m_events;

		EventModel* startEvent() { return m_events[0]; }
		EventModel* endEvent() { return m_events[1]; }
		
		int m_nextIntervalId{};
};

