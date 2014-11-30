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

	public:
		ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
								 iscore::ProcessViewInterface* view,
								 QObject* parent);
		virtual ~ScenarioProcessPresenter() = default;


	signals:
		void submitCommand(iscore::SerializableCommand*);

	public slots:
		// Model -> view
		void on_eventCreated(int eventId);
		void on_intervalCreated(int intervalId);
		void on_eventDeleted(int eventId);
		void on_intervalDeleted(int intervalId);

		// View -> Command
		void on_scenarioPressed(QPointF);

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_intervalCreated_impl(IntervalModel* interval_model);


		ScenarioProcessViewModel* m_viewModel;
		ScenarioProcessView* m_view;

		std::vector<IntervalPresenter*> m_intervals;
		std::vector<EventPresenter*> m_events;
};

