#pragma once
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

namespace OSSIA
{
	class Scenario;
}
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

		friend QDataStream& operator <<(QDataStream& s, const ScenarioProcessSharedModel& scenario);
		friend QDataStream& operator >>(QDataStream& s, ScenarioProcessSharedModel& scenario);
	public:
		using view_model_type = AbstractScenarioProcessViewModel;

		ScenarioProcessSharedModel(int id, QObject* parent);
		ScenarioProcessSharedModel(QDataStream& data, QObject* parent);
		virtual ~ScenarioProcessSharedModel() = default;


		virtual ProcessViewModelInterface* makeViewModel(int viewModelId, QObject* parent) override;
		virtual ProcessViewModelInterface* makeViewModel(QDataStream&, QObject* parent) override;

		void makeViewModel_impl(view_model_type*);

		virtual QString processName() const override
		{
			return "Scenario";
		}

		// Creation of objects.

		// TODO clean-up to keep only createConstraintAndEndEventFromEvent. Put the others in Commands.
		// Creates an constraint between two pre-existing events
		void createConstraintBetweenEvents(int startEventId, int endEventId, int newConstraintModelId);

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
												  int newEventId);

		void moveEventAndConstraint(int eventId, int time, double heightPosition);
		void moveConstraint(int constraintId, int deltaX, double heightPosition);
		void moveNextElements(int firstEventMovedId, int deltaTime, QVector<int> &movedEvent);


		void undo_createConstraintBetweenEvents(int constraintId);
		void undo_createConstraintAndEndEventFromEvent(int constraintId);
		void undo_createConstraintAndEndEventFromStartEvent(int constraintId);
		void undo_createConstraintAndBothEvents(int constraintId);



		ConstraintModel* constraint(int constraintId);
		EventModel* event(int eventId);

		EventModel* startEvent();
		EventModel* endEvent();

		// For the presenter :
		// TODO pass them by copy instead. It will be less painful.
		const std::vector<ConstraintModel*> constraints() const
		{ return m_constraints; }
		const std::vector<EventModel*> events() const
		{ return m_events; }

	signals:
		void eventCreated(int eventId);
		void constraintCreated(int constraintId);
		void eventDeleted(int eventId);
		void constraintRemoved(int constraintId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);

	protected:
		virtual void serialize(QDataStream&) const override;
		virtual void deserialize(QDataStream&) override;

	private:
		OSSIA::Scenario* m_scenario;

		std::vector<ConstraintModel*> m_constraints;
		std::vector<EventModel*> m_events;

		int m_startEventId{};
		int m_endEventId{};
};
