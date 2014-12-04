#include "EventPresenter.hpp"
#include "EventModel.hpp"
#include "EventView.hpp"
#include <QPointF>
#include <QGraphicsScene>

EventPresenter::EventPresenter(EventModel* model,
							   EventView* view,
							   QObject* parent):
	QNamedObject{parent, "EventPresenter"},
	m_model{model},
	m_view{view}
{
	// The scenario catches this :
	connect(m_view, &EventView::eventPressedWithControl,
			[this] () { emit eventSelected(id()); });
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
