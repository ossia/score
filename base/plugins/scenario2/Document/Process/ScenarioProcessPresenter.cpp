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

#include "Commands/Scenario/CreateEventCommand.hpp"
#include <QDebug>

#include "utilsCPP11.hpp"

ScenarioProcessPresenter::ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
												   iscore::ProcessViewInterface* view,
												   QObject* parent):
	iscore::ProcessPresenterInterface{parent, "ScenarioProcessPresenter"},
	m_viewModel{static_cast<ScenarioProcessViewModel*>(model)},
	m_view{static_cast<ScenarioProcessView*>(view)}
{
	/////// Setup of existing data
	// TODO Question :
	// étirement temporel d'une boîte qui contient un scénario hiérarchique ?
	// veut on étirer les choses ou les laisser à leur place ?
	// For each interval & event, display' em
	for(auto interval_model : m_viewModel->model()->intervals())
	{
		on_intervalCreated_impl(interval_model);
	}

	for(auto event_model : m_viewModel->model()->events())
	{
		on_eventCreated_impl(event_model);
	}

	/////// Connections
	connect(this, SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));

	connect(m_view, SIGNAL(scenarioPressed(QPointF)),
			this, SLOT(on_scenarioPressed(QPointF)));

	connect(m_viewModel, SIGNAL(eventCreated(int)), this, SLOT(on_eventCreated(int)));
	connect(m_viewModel, SIGNAL(eventDeleted(int)), this, SLOT(on_eventDeleted(int)));
	connect(m_viewModel, SIGNAL(intervalCreated(int)), this, SLOT(on_intervalCreated(int)));
	connect(m_viewModel, SIGNAL(intervalDeleted(int)), this, SLOT(on_intervalDeleted(int)));
}

void ScenarioProcessPresenter::on_eventCreated(int eventId)
{
	on_eventCreated_impl(m_viewModel->model()->event(eventId));
}

void ScenarioProcessPresenter::on_intervalCreated(int intervalId)
{
	on_intervalCreated_impl(m_viewModel->model()->interval(intervalId));
}

// Todo : deduplicate with a templates
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





void ScenarioProcessPresenter::on_scenarioPressed(QPointF point)
{
	qDebug(Q_FUNC_INFO);
	qDebug() << point.x();
	auto cmd = new CreatEventCommand(ObjectPath::pathFromObject("BaseIntervalModel", m_viewModel->model()), point.x());
	submitCommand(cmd);
}




void ScenarioProcessPresenter::on_eventCreated_impl(EventModel* event_model)
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

void ScenarioProcessPresenter::on_intervalCreated_impl(IntervalModel* interval_model)
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
