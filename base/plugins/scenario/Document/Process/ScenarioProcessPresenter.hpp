#pragma once
#include <interface/process/ProcessPresenterInterface.hpp>
namespace iscore
{
	class ProcessViewModelInterface;
	class ProcessViewInterface;
	class SerializableCommand;
}
class IntervalPresenter;
class EventPresenter;
class ScenarioProcessViewModel;
class ScenarioProcessView;
class EventModel;
class IntervalModel;

class ScenarioProcessPresenter : public iscore::ProcessPresenterInterface
{
	Q_OBJECT

		Q_PROPERTY(int currentlySelectedEvent
				   READ currentlySelectedEvent
				   WRITE setCurrentlySelectedEvent
				   NOTIFY currentlySelectedEventChanged)

	public:
		ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
								 iscore::ProcessViewInterface* view,
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

		void on_intervalCreated(int intervalId);
		void on_intervalDeleted(int intervalId);
		void on_intervalMoved(int intervalId);

		// View -> Command
		void on_scenarioPressed(QPointF);
		void on_scenarioReleased(QPointF);

	private slots:
		void setCurrentlySelectedEvent(int arg);
		void createIntervalAndEventFromEvent(int id, int distance, double heightPos);
		void moveEventAndInterval(int id, int distance, double heightPos);
		void moveIntervalOnVertical(int id, double heightPos);

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_intervalCreated_impl(IntervalModel* interval_model);


		ScenarioProcessViewModel* m_viewModel;
		ScenarioProcessView* m_view;

		std::vector<IntervalPresenter*> m_intervals;
		std::vector<EventPresenter*> m_events;

		int m_currentlySelectedEvent{};
};

