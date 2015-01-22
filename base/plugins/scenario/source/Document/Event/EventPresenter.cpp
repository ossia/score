#include "EventPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"

#include <QPointF>
#include <QGraphicsScene>

EventPresenter::EventPresenter(EventModel* model,
							   EventView* view,
							   QObject* parent):
	NamedObject{"EventPresenter", parent},
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
			this, &EventPresenter::on_eventReleasedWithControl);

	connect(m_view, &EventView::eventReleased,
			this, &EventPresenter::on_eventReleased);
}

EventPresenter::~EventPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

id_type<EventModel> EventPresenter::id() const
{
	return m_model->id();
}

EventView *EventPresenter::view() const
{
	return m_view;
}

EventModel* EventPresenter::model() const
{
	return m_model;
}

bool EventPresenter::isSelected() const
{
	return m_view->isSelected();
}

void EventPresenter::deselect()
{
	m_view->setSelected(false);
}

void EventPresenter::on_eventReleased(QPointF p)
{
	EventData d{};
	d.eventClickedId = id();
	d.x = p.x();
	d.y = p.y();
	emit eventReleased(d);
}

void EventPresenter::on_eventReleasedWithControl(QPointF p, QPointF pInScene)
{
	EventData d{};
	d.eventClickedId = id();
	d.x = p.x();
	d.y = p.y();
	d.scenePos = pInScene;
	emit eventReleasedWithControl(d);
}
