#include "AbstractConstraintView.hpp"
#include "AbstractConstraintPresenter.hpp"
AbstractConstraintView::AbstractConstraintView(
        AbstractConstraintPresenter& presenter,
        QGraphicsItem *parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    setAcceptHoverEvents(true);
    m_dashPen.setDashPattern({2., 4.});
}

void AbstractConstraintView::setInfinite(bool infinite)
{
    prepareGeometryChange();

    m_infinite = infinite;
    update();
}

void AbstractConstraintView::setDefaultWidth(double width)
{
    prepareGeometryChange();
    m_defaultWidth = width;
}

void AbstractConstraintView::setMaxWidth(bool infinite, double max)
{
    prepareGeometryChange();

    setInfinite(infinite);
    if(!infinite)
    {
        //qDebug() << max;
        m_maxWidth = max;
    }

}

void AbstractConstraintView::setMinWidth(double min)
{
    prepareGeometryChange();
    m_minWidth = min;
}

void AbstractConstraintView::setHeight(double height)
{
    prepareGeometryChange();
    m_height = height;
}

void AbstractConstraintView::setPlayWidth(double width)
{
    m_playWidth = width;
    update();
}

void AbstractConstraintView::setValid(bool val)
{
    m_validConstraint = val;
}

#include <QGraphicsSceneMouseEvent>
void AbstractConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.pressed(event->scenePos());
}

void AbstractConstraintView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void AbstractConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}
bool AbstractConstraintView::warning() const
{
    return m_warning;
}

void AbstractConstraintView::setWarning(bool warning)
{
    m_warning = warning;
}

