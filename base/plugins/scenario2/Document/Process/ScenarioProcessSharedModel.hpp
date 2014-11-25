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
		
		void createInterval(int dur); // Depuis start.
		void createInterval(EventModel* start, int dur);
		
		IntervalModel* interval(int intervalId);
		
	private:
		std::vector<IntervalModel*> m_intervals;
		std::vector<EventModel*> m_events;

		EventModel* m_startEvent{};
		EventModel* m_endEvent{};
		
		int m_nextIntervalId{};
};

