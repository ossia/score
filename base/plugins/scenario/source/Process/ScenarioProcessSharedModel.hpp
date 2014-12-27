#pragma once
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ScenarioProcessSharedModelSerialization.hpp"

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

		friend void Visitor<Reader<DataStream>>::visit<const ScenarioProcessSharedModel>(const ScenarioProcessSharedModel&);
		friend void Visitor<Writer<DataStream>>::visit<ScenarioProcessSharedModel>(ScenarioProcessSharedModel&);

	public:
		using view_model_type = AbstractScenarioProcessViewModel;

		ScenarioProcessSharedModel(int id, QObject* parent);

		template<typename Impl>
		ScenarioProcessSharedModel(Deserializer<Impl>& vis, QObject* parent):
			ProcessSharedModelInterface{vis, parent}
		{
			vis.visit(*this);
		}


		virtual ~ScenarioProcessSharedModel() = default;


		virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
														 QObject* parent) override;

		virtual ProcessViewModelInterface* makeViewModel(SerializationIdentifier identifier,
														 void* data,
														 QObject* parent) override;

		void makeViewModel_impl(view_model_type*);

		virtual QString processName() const override
		{
			return "Scenario";
		}

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

		// Re-creation. Note : only using these methods might let the Scenario in an incoherent state.
		// To use in deserialization only (maybe put it there ?).
		//void createConstraint(QDataStream&);
		//void createEvent(QDataStream&);
		void addConstraint(ConstraintModel* constraint);
		void addEvent(EventModel* event);

		void moveEventAndConstraint(int eventId, int time, double heightPosition);
		void moveConstraint(int constraintId, int deltaX, double heightPosition);
		void moveNextElements(int firstEventMovedId, int deltaTime, QVector<int> &movedEvent);

		void undo_createConstraintBetweenEvents(int constraintId);
		void undo_createConstraintAndEndEventFromEvent(int constraintId);



		// Accessors
		ConstraintModel* constraint(int constraintId) const;
		EventModel* event(int eventId) const;

		EventModel* startEvent() const;
		EventModel* endEvent() const;

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
		//virtual void serialize(QDataStream&) const override;
		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const override;

	private:
		OSSIA::Scenario* m_scenario;

		std::vector<ConstraintModel*> m_constraints;
		std::vector<EventModel*> m_events;

		int m_startEventId{};
		int m_endEventId{};
};
