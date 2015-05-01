#include "EventPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"

#include <QGraphicsScene>

EventPresenter::EventPresenter(const EventModel& model,
                               QGraphicsObject* parentview,
                               QObject* parent) :
    NamedObject {"EventPresenter", parent},
    m_model {model},
    m_view {new EventView{*this, parentview}}
{
    // The scenario catches this :
    connect(&m_model.selection, &Selectable::changed,
            m_view, &EventView::setSelected);

    connect(&(m_model.metadata),  &ModelMetadata::colorChanged,
            m_view,                 &EventView::changeColor);

    connect(&m_model, &EventModel::heightPercentageChanged,
            this,    &EventPresenter::heightPercentageChanged);
    connect(&m_model, &EventModel::conditionChanged,
            m_view,  &EventView::setCondition);

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

const id_type<EventModel>& EventPresenter::id() const
{
    return m_model.id();
}

EventView* EventPresenter::view() const
{
    return m_view;
}

const EventModel& EventPresenter::model() const
{
    return m_model;
}

bool EventPresenter::isSelected() const
{
    return m_model.selection.get();
}
