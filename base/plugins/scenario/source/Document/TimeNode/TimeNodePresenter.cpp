#include "TimeNodePresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>

#include "Document/Event/EventData.hpp"

TimeNodePresenter::TimeNodePresenter(TimeNodeModel *model,
									 TimeNodeView *view,
									 QObject *parent):
	NamedObject{"TimeNodePresenter", parent},
	m_model{model},
	m_view{view}
{
    connect(m_view, &TimeNodeView::timeNodeReleased,
            this,   &TimeNodePresenter::on_timeNodeReleased);

    connect(m_view, &TimeNodeView::timeNodeSelected,
            [&] ()
        {
            emit elementSelected(m_model);
        });
}

TimeNodePresenter::~TimeNodePresenter()
{
    if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
		m_view->deleteLater();
    }
}

id_type<TimeNodeModel> TimeNodePresenter::id() const
{
	return m_model->id();
}

TimeNodeModel *TimeNodePresenter::model()
{
	return m_model;
}

TimeNodeView *TimeNodePresenter::view()
{
    return m_view;
}

void TimeNodePresenter::on_timeNodeReleased(QPointF p)
{
    EventData d{};
    d.eventClickedId = model()->events().first();
    d.x = p.x();
    emit timeNodeReleased(d);
}
