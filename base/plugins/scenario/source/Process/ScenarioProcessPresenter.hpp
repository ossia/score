#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"

namespace iscore
{
	class SerializableCommand;
}
class ProcessViewModelInterface;
class ProcessViewInterface;

class ConstraintPresenter;
class EventPresenter;
class ScenarioProcessViewModel;
class ScenarioProcessView;
class EventModel;
class ConstraintModel;
struct EventData;

class ScenarioProcessPresenter : public ProcessPresenterInterface
{
	Q_OBJECT

		Q_PROPERTY(int currentlySelectedEvent
				   READ currentlySelectedEvent
				   WRITE setCurrentlySelectedEvent
				   NOTIFY currentlySelectedEventChanged)

	public:
		ScenarioProcessPresenter(ProcessViewModelInterface* model,
								 ProcessViewInterface* view,
								 QObject* parent);
		virtual ~ScenarioProcessPresenter() = default;


		virtual int id() const;
		int currentlySelectedEvent() const;

	signals:
		void currentlySelectedEventChanged(int arg);

	public slots:
		// Model -> view
		void on_eventCreated(int eventId);
		void on_eventDeleted(int eventId);
		void on_eventMoved(int eventId);

		void on_constraintCreated(int constraintId);
		void on_constraintDeleted(int constraintId);
		void on_constraintMoved(int constraintId);

		// View -> Command
		void on_scenarioPressed(QPointF);
		void on_scenarioReleased(QPointF);

	private slots:
		void setCurrentlySelectedEvent(int arg);
        void createConstraintAndEventFromEvent(EventData data);
        void moveEventAndConstraint(EventData data);
        void moveConstraintOnVertical(int id, double verticalMove);

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_constraintCreated_impl(ConstraintModel* constraint_model);


		ScenarioProcessViewModel* m_viewModel;
		ScenarioProcessView* m_view;

		std::vector<ConstraintPresenter*> m_constraints;
		std::vector<EventPresenter*> m_events;

		int m_currentlySelectedEvent{};
};

