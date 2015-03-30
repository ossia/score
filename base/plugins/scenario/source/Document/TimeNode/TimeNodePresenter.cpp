#include "TimeNodePresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>

#include "Document/Event/EventData.hpp"

TimeNodePresenter::TimeNodePresenter(TimeNodeModel* model,
                                     TimeNodeView* view,
                                     QObject* parent) :
    NamedObject {"TimeNodePresenter", parent},
    m_model {model},
    m_view {view}
{
    connect(&m_model->selection, &Selectable::changed,
            m_view, &TimeNodeView::setSelected);

    connect(m_view, &TimeNodeView::timeNodePressed,
            this,   &TimeNodePresenter::pressed);
    connect(m_view, &TimeNodeView::timeNodeMoved,
            this,   &TimeNodePresenter::on_timeNodeMoved);
    connect(m_view, &TimeNodeView::timeNodeReleased,
            this,   &TimeNodePresenter::timeNodeReleased);

    connect(m_model, &TimeNodeModel::newEvent,
            this,    &TimeNodePresenter::on_eventAdded);

    connect(&(m_model->metadata),  &ModelMetadata::colorChanged,
            m_view,                &TimeNodeView::changeColor);

}

TimeNodePresenter::~TimeNodePresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

id_type<TimeNodeModel> TimeNodePresenter::id() const
{
    return m_model->id();
}

TimeNodeModel* TimeNodePresenter::model()
{
    return m_model;
}

TimeNodeView* TimeNodePresenter::view()
{
    return m_view;
}

void TimeNodePresenter::on_timeNodeMoved(QPointF p)
{
    EventData d {};
    d.eventClickedId = model()->events().first();
    d.x = p.x();
    emit timeNodeMoved(d);
}

void TimeNodePresenter::on_eventAdded(id_type<EventModel> eventId)
{
    qDebug() << "receive" << this->id() << eventId;
    emit eventAdded(eventId, this->model()->id());
}
