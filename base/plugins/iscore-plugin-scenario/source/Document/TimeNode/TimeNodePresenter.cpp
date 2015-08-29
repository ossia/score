#include "TimeNodePresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>
#include <QGraphicsObject>
#include <iscore/widgets/GraphicsItem.hpp>

TimeNodePresenter::TimeNodePresenter(const TimeNodeModel& model,
                                     QGraphicsObject *parentview,
                                     QObject* parent) :
    NamedObject {"TimeNodePresenter", parent},
    m_model {model},
    m_view {new TimeNodeView{*this, parentview}}
{
    con(m_model.selection, &Selectable::changed,
            m_view, &TimeNodeView::setSelected);

    con(m_model, &TimeNodeModel::newEvent,
            this,     &TimeNodePresenter::on_eventAdded);

    con((m_model.metadata), &ModelMetadata::colorChanged,
            m_view,               &TimeNodeView::changeColor);

    // TODO find a correct way to handle validity of model elements.
    // extentChanged is updated in scenario.
}

TimeNodePresenter::~TimeNodePresenter()
{
    deleteGraphicsObject(m_view);
}

const Id<TimeNodeModel>& TimeNodePresenter::id() const
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

void TimeNodePresenter::on_eventAdded(const Id<EventModel>& eventId)
{
    emit eventAdded(eventId, m_model.id());
}


#include <iscore/tools/NotifyingMap.hpp>

