#include "ScenarioProcessPresenter.hpp"
#include "ScenarioProcessSharedModel.hpp"
#include "ScenarioProcessViewModel.hpp"
#include "ScenarioProcessView.hpp"

#include "Interval/IntervalView.hpp"
#include "Interval/IntervalPresenter.hpp"
#include "Interval/IntervalModel.hpp"
#include "Interval/IntervalContent/IntervalContentView.hpp"
#include "Interval/IntervalContent/IntervalContentPresenter.hpp"

#include "Event/EventModel.hpp"
#include "Event/EventPresenter.hpp"
#include "Event/EventView.hpp"
#include <QDebug>

ScenarioProcessPresenter::ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
												   iscore::ProcessViewInterface* view,
												   QObject* parent):
	iscore::ProcessPresenterInterface{parent, "ScenarioProcessPresenter"},
	m_model{static_cast<ScenarioProcessViewModel*>(model)},
	m_view{static_cast<ScenarioProcessView*>(view)}
{
	qDebug(Q_FUNC_INFO);

	// TODO Question : étirement temporel d'une boîte qui contient un scénario hiérarchique ? veut on étirer les choses ou les laisser à leur place ?
	// For each interval & event, display' em
	for(auto interval_model : m_model->model()->intervals())
	{
		auto interval_view = new IntervalView{view};
		auto interval_presenter = new IntervalPresenter{interval_model,
														interval_view,
														this};

		interval_view->m_rect.setX(m_view->boundingRect().x() + interval_model->m_x);

		auto interval_y = m_view->boundingRect().y() + m_view->boundingRect().height() * interval_model->heightPercentage();
		interval_view->m_rect.setY(interval_y);

		m_intervals.push_back(interval_presenter);
	}

	for(auto event_model : m_model->model()->events())
	{
		auto event_view = new EventView{view};
		auto event_presenter = new EventPresenter{event_model,
												  event_view,
												  this};

		event_view->m_rect.setX(m_view->boundingRect().x() + event_model->m_x - ((event_model->m_x)? event_view->m_rect.width() / 2. : 0));

		auto event_y = m_view->boundingRect().y() + m_view->boundingRect().height() * event_model->heightPercentage();
		event_view->m_rect.setY(event_y);

		m_events.push_back(event_presenter);
	}
}
