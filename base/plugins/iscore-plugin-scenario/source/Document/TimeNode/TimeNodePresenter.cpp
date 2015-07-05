#include "TimeNodePresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>
#include <QGraphicsObject>

TimeNodePresenter::TimeNodePresenter(const TimeNodeModel& model,
                                     QGraphicsObject *parentview,
                                     QObject* parent) :
    NamedObject {"TimeNodePresenter", parent},
    m_model {model},
    m_view {new TimeNodeView{*this, parentview}}
{
    connect(&m_model.selection, &Selectable::changed,
            m_view, &TimeNodeView::setSelected);

    connect(&m_model, &TimeNodeModel::newEvent,
            this,     &TimeNodePresenter::on_eventAdded);

    connect(&(m_model.metadata), &ModelMetadata::colorChanged,
            m_view,               &TimeNodeView::changeColor);

    connect(&m_model,   &TimeNodeModel::timeNodeValid,
            m_view, &TimeNodeView::setValid);

    connect(&m_model, &TimeNodeModel::extentChanged,
            this, [&] (const VerticalExtent& extent) {
        m_view->setPos({m_view->pos().x(),
                        extent.top * parentview->boundingRect().height()});
        m_view->setExtent(extent.top * parentview->boundingRect().height(),
                          extent.bottom * parentview->boundingRect().height());
    });

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

const id_type<TimeNodeModel>& TimeNodePresenter::id() const
{
    return m_model.id();
}

const TimeNodeModel& TimeNodePresenter::model() const
{
    return m_model;
}

TimeNodeView* TimeNodePresenter::view() const
{
    return m_view;
}

void TimeNodePresenter::on_eventAdded(const id_type<EventModel>& eventId)
{
    emit eventAdded(eventId, m_model.id());
}
