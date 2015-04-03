#include "EventPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"

#include <QPointF>
#include <QGraphicsScene>

EventPresenter::EventPresenter(EventModel* model,
                               EventView* view,
                               QObject* parent) :
    NamedObject {"EventPresenter", parent},
    m_model {model},
    m_view {view}
{
    // The scenario catches this :
    connect(m_view, &EventView::eventPressed,
            this,   &EventPresenter::pressed);

    connect(&m_model->selection, &Selectable::changed,
            m_view, &EventView::setSelected);


    connect(m_view, &EventView::eventMoved,
            [this](QPointF p)
    {
        emit eventMoved(pointToEventData(p));
    });

    connect(m_view, &EventView::eventReleased,
            this,	&EventPresenter::eventReleased);

    connect(m_view, &EventView::ctrlStateChanged,
            this,	&EventPresenter::ctrlStateChanged);

    connect(& (m_model->metadata),  &ModelMetadata::colorChanged,
            m_view,                 &EventView::changeColor);

    connect(m_view, &EventView::eventHoverEnter,
            this,   &EventPresenter::eventHoverEnter);

    connect(m_view, &EventView::eventHoverLeave,
            this,   &EventPresenter::eventHoverLeave);
}

EventPresenter::~EventPresenter()
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

id_type<EventModel> EventPresenter::id() const
{
    return m_model->id();
}

EventView* EventPresenter::view() const
{
    return m_view;
}

EventModel* EventPresenter::model() const
{
    return m_model;
}

bool EventPresenter::isSelected() const
{
    return m_model->selection.get();
}

EventData EventPresenter::pointToEventData(QPointF p) const
{
    EventData d {};
    d.eventClickedId = id();
    d.x = p.x();
    d.y = p.y();
    d.scenePos = view()->mapToScene(p);
    return d;
}
