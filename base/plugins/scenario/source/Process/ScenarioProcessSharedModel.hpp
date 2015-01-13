#pragma once
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ScenarioProcessSharedModelSerialization.hpp"

namespace OSSIA
{
	class Scenario;
}
class TimeNodeModel;
class ConstraintModel;
class EventModel;
class AbstractScenarioProcessViewModel;

/**
 * @brief The ScenarioProcessSharedModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioProcessSharedModel : public ProcessSharedModelInterface
{
		Q_OBJECT

		friend void Visitor<Reader<DataStream>>::readFrom<ScenarioProcessSharedModel>(const ScenarioProcessSharedModel&);
		friend void Visitor<Writer<DataStream>>::writeTo<ScenarioProcessSharedModel>(ScenarioProcessSharedModel&);
		friend class ScenarioProcessFactory;

	public:
		using view_model_type = AbstractScenarioProcessViewModel;

		ScenarioProcessSharedModel(int id, QObject* parent);

		virtual ~ScenarioProcessSharedModel();
		virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
														 QObject* parent) override;

		virtual QString processName() const override
		{
			return "Scenario";
		}

		// High-level operations (maintaining consistency)
		/**
		 * @brief createConstraintBetweenEvents
		 *
		 * Creates a new constraint between two existing events
		 */
		void createConstraintBetweenEvents(int startEventId,
										   int endEventId,
										   int newConstraintModelId);

		/**
		 * @brief createConstraintAndEndEventFromEvent Base building block of a scenario.
		 *
		 * Given a starting event and a duration, creates an constraint and an event where
		 * the constraint is linked to both events.
		 */
        void createConstraintAndEndEventFromEvent(int startEventId,
                                                  int duration,
                                                  double heightPos,
                                                  int newConstraintId,
                                                  int newEventId,
                                                  int newTimeNodeId);


		void moveEventAndConstraint(int eventId, int time, double heightPosition);
		void moveConstraint(int constraintId, int deltaX, double heightPosition);
		void moveNextElements(int firstEventMovedId, int deltaTime, QVector<int> &movedEvent);


		// Low-level operations (the caller has the responsibility to maintain the consistency of the scenario)
		void addConstraint(ConstraintModel* constraint);
		void addEvent(EventModel* event);

		void removeConstraint(int constraintId);
		void removeEvent(int eventId);
        void removeEventFromTimeNode(int eventId);
		void undo_createConstraintAndEndEventFromEvent(int constraintId);
        void undo_createConstraintBetweenEvent(int constraintId);


		// Accessors
		ConstraintModel* constraint(int constraintId) const;
		EventModel* event(int eventId) const;
        TimeNodeModel* timeNode(int timeNodeId) const;

		EventModel* startEvent() const;
		EventModel* endEvent() const;

		// Here, a copy is returned because it might be possible
		// to call a method on the scenario (e.g. removeConstraint) that changes the vector
		// while iterating, which would invalidate the iterators
		// and lead to undefined behaviour
		std::vector<ConstraintModel*> constraints() const
		{ return m_constraints; }
		std::vector<EventModel*> events() const
		{ return m_events; }
        std::vector<TimeNodeModel*> timeNodes() const
        { return m_timeNodes; }

	signals:
		void eventCreated(int eventId);
		void constraintCreated(int constraintId);
        void timeNodeCreated(int timeNodeId);
		void eventRemoved(int eventId);
		void constraintRemoved(int constraintId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);

		void locked();
		void unlocked();

	public slots:
		void lock()
		{ emit locked(); }
		void unlock()
		{ emit unlocked(); }

		void on_viewModelDestroyed(QObject*);

	protected:
		template<typename Impl>
		ScenarioProcessSharedModel(Deserializer<Impl>& vis, QObject* parent):
			ProcessSharedModelInterface{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ProcessViewModelInterface* makeViewModel(SerializationIdentifier identifier,
														 void* data,
														 QObject* parent) override;

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const override;

	private:
		void makeViewModel_impl(view_model_type*);

		OSSIA::Scenario* m_scenario;

		std::vector<ConstraintModel*> m_constraints;
		std::vector<EventModel*> m_events;
        std::vector<TimeNodeModel*> m_timeNodes;

		int m_startEventId{};
		int m_endEventId{};
};
