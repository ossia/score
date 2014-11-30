#include "EventPresenter.hpp"
#include "EventModel.hpp"
#include "EventView.hpp"
#include <QPointF>


EventPresenter::EventPresenter(EventModel* model,
							   EventView* view,
							   QObject* parent):
	QNamedObject{parent, "EventPresenter"},
	m_model{model},
	m_view{view}
{
	// The scenario catches this :
	connect(m_view, &EventView::eventPressed,
			[this] () { emit eventSelected(id()); });
	connect(m_view, &EventView::eventReleased,
			[&] (QPointF p)
	{
		emit eventReleased(id(), p.x());
	});
}

EventPresenter::~EventPresenter()
{
	delete m_view;
}

int EventPresenter::id() const
{
	return m_model->id();
}