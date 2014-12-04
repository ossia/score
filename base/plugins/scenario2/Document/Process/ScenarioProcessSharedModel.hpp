#pragma once
#include <interface/process/ProcessSharedModelInterface.hpp>

namespace OSSIA
{
	class Scenario;
}
class IntervalModel;
class EventModel;
/**
 * @brief The ScenarioProcessSharedModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioProcessSharedModel : public iscore::ProcessSharedModelInterface
{
	Q_OBJECT

	public:
		ScenarioProcessSharedModel(int id, QObject* parent);
		ScenarioProcessSharedModel(QDataStream& data, QObject* parent);
		virtual ~ScenarioProcessSharedModel() = default;
		virtual iscore::ProcessViewModelInterface* makeViewModel(int viewModelId, int processId, QObject* parent) override;
		virtual iscore::ProcessViewModelInterface* makeViewModel(QDataStream&, QObject* parent) override;

		virtual QString processName() const override
		{
			return "Scenario";
		}

		virtual void serialize(QDataStream&) const override;
		virtual void deserialize(QDataStream&) override;

		// Creation of objects.

		// Creates an interval between two pre-existing events
		int createIntervalBetweenEvents(int startEventId, int endEventId);

		/**
		 * @brief createIntervalAndEndEventFromEvent Base building block of a scenario.
		 * @param startEventId Identifier of the start event of the new interval
		 * @param duration duration of the new interval
		 * @return A pair : <new interval id, new event id>
		 *
		 * Given a starting event and a duration, creates an interval and an event where
		 * the interval is linked to both events.
		 */
        std::tuple<int, int> createIntervalAndEndEventFromEvent(int startEventId,
                                                               int duration, double heightPos);

		// Creates an interval between the start event of the scenario and this one
		// and an event at the end of this interval
        std::tuple<int, int> createIntervalAndEndEventFromStartEvent(int time, double heightPos);

		// Creates :
		/// - An interval from the start event of the scenario to an event at startTime
		/// - The event at startTime
		/// - An interval going from the event at startTime to the event at startTime + duration
		/// - The event at startTime + duration
        std::tuple<int, int, int, int> createIntervalAndBothEvents(int startTime,
                                                                   int duration, double heightPos);


		void undo_createIntervalBetweenEvents(int intervalId);
		void undo_createIntervalAndEndEventFromEvent(int intervalId);
		void undo_createIntervalAndEndEventFromStartEvent(int intervalId);
		void undo_createIntervalAndBothEvents(int intervalId);



		IntervalModel* interval(int intervalId);
		EventModel* event(int eventId);

		// For the presenter :
		const std::vector<IntervalModel*> intervals() const
		{ return m_intervals; }
		const std::vector<EventModel*> events() const
		{ return m_events; }

	signals:
		void eventCreated(int eventId);
		void intervalCreated(int intervalId);
		void eventDeleted(int eventId);
		void intervalDeleted(int intervalId);

	private:
		OSSIA::Scenario* m_scenario;

		std::vector<IntervalModel*> m_intervals;
		std::vector<EventModel*> m_events;

		EventModel* startEvent() { return m_events[0]; }
		EventModel* endEvent() { return m_events[1]; }
};

