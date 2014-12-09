#include "EventPresenter.hpp"
#include "EventModel.hpp"
#include "EventView.hpp"
#include <QPointF>
#include <QGraphicsScene>

EventPresenter::EventPresenter(EventModel* model,
							   EventView* view,
							   QObject* parent):
	NamedObject{parent, "EventPresenter"},
	m_model{model},
	m_view{view}
{
	// The scenario catches this :
	connect(m_view, &EventView::eventPressed,
			[&] ()
	{
		emit eventSelected(id());
		emit elementSelected(m_model);
	});
	connect(m_view, &EventView::eventReleasedWithControl,
			[&] (QPointF p)
	{
		emit eventReleasedWithControl(id(), p.x(), p.y());
	});
	connect(m_view, &EventView::eventReleased,
			[&] (QPointF p)
	{
		emit eventReleased(id(), p.x(), p.y());
	});
}

EventPresenter::~EventPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

int EventPresenter::id() const
{
	return m_model->id();
}

EventView *EventPresenter::view() const
{
	return m_view;
}

EventModel *EventPresenter::model() const
{
	return m_model;
}
