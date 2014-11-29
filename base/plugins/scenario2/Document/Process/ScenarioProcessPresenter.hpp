#pragma once
#include <interface/process/ProcessPresenterInterface.hpp>
namespace iscore
{
	class ProcessViewModelInterface;
	class ProcessViewInterface;
}
class IntervalPresenter;
class EventPresenter;
class ScenarioProcessViewModel;
class ScenarioProcessView;
class ScenarioProcessPresenter : public iscore::ProcessPresenterInterface
{
	Q_OBJECT

	public:
		ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
								 iscore::ProcessViewInterface* view,
								 QObject* parent);
		virtual ~ScenarioProcessPresenter() = default;

	private:
		ScenarioProcessViewModel* m_model;
		ScenarioProcessView* m_view;

		std::vector<IntervalPresenter*> m_intervals;
		std::vector<EventPresenter*> m_events;
};

