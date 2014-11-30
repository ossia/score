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
	/////// Setup of existing data
	// TODO Question :
	// étirement temporel d'une boîte qui contient un scénario hiérarchique ?
	// veut on étirer les choses ou les laisser à leur place ?
	// For each interval & event, display' em
	for(auto interval_model : m_model->model()->intervals())
	{
		on_intervalCreated_sub(interval_model);
	}

	for(auto event_model : m_model->model()->events())
	{
		on_eventCreated_sub(event_model);
	}

	/////// Connections
	connect(m_model, SIGNAL(eventCreated(int)), this, SLOT(on_eventCreated(int)));
	connect(m_model, SIGNAL(eventDeleted(int)), this, SLOT(on_eventDeleted(int)));
	connect(m_model, SIGNAL(intervalCreated(int)), this, SLOT(on_intervalCreated(int)));
	connect(m_model, SIGNAL(intervalDeleted(int)), this, SLOT(on_intervalDeleted(int)));
}

void ScenarioProcessPresenter::on_eventCreated(int eventId)
{
	on_eventCreated_sub(m_model->model()->event(eventId));
}

void ScenarioProcessPresenter::on_intervalCreated(int intervalId)
{
	on_intervalCreated_sub(m_model->model()->interval(intervalId));
}

void ScenarioProcessPresenter::on_eventDeleted(int eventId)
{
	auto it = std::find_if(std::begin(m_events),
						   std::end(m_events),
						   [eventId] (EventPresenter const * pres)
	{
		return pres->id() == eventId;
	});

	if(it != std::end(m_events))
	{
		delete *it;
		m_events.erase(it);
	}
}

void ScenarioProcessPresenter::on_intervalDeleted(int intervalId)
{
	auto it = std::find_if(std::begin(m_intervals),
						   std::end(m_intervals),
						   [intervalId] (IntervalPresenter const * pres)
	{
		return pres->id() == intervalId;
	});

	if(it != std::end(m_intervals))
	{
		delete *it;
		m_intervals.erase(it);
	}
}

void ScenarioProcessPresenter::on_eventCreated_sub(EventModel* event_model)
{
	auto rect = m_view->boundingRect();

	auto event_view = new EventView{m_view};
	auto event_presenter = new EventPresenter{event_model,
						   event_view,
						   this};

	event_view->setTopLeft({rect.x() + event_model->m_x,
							rect.y() + rect.height() * event_model->heightPercentage()});

	m_events.push_back(event_presenter);
}

void ScenarioProcessPresenter::on_intervalCreated_sub(IntervalModel* interval_model)
{
	auto rect = m_view->boundingRect();

	auto interval_view = new IntervalView{m_view};
	auto interval_presenter = new IntervalPresenter{interval_model,
													interval_view,
													this};

	interval_view->setTopLeft({rect.x() + interval_model->m_x,
							   rect.y() + rect.height() * interval_model->heightPercentage()});

	m_intervals.push_back(interval_presenter);
}
