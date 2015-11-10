#include "ConstraintView.hpp"
#include "ConstraintPresenter.hpp"
#include <Scenario/Document/State/StateView.hpp>

#include <QGraphicsSceneMouseEvent>

ConstraintView::ConstraintView(
        ConstraintPresenter& presenter,
        QGraphicsItem *parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    setAcceptHoverEvents(true);
    m_dashPen.setDashPattern({2., 4.});
}

ConstraintView::~ConstraintView()
{

}

void ConstraintView::setInfinite(bool infinite)
{
    prepareGeometryChange();

    m_infinite = infinite;
    update();
}

void ConstraintView::setDefaultWidth(double width)
{
    prepareGeometryChange();
    m_defaultWidth = width;
}

void ConstraintView::setMaxWidth(bool infinite, double max)
{
    prepareGeometryChange();

    setInfinite(infinite);
    if(!infinite)
    {
        m_maxWidth = max;
    }

}

void ConstraintView::setMinWidth(double min)
{
    prepareGeometryChange();
    m_minWidth = min;
}

void ConstraintView::setHeight(double height)
{
    prepareGeometryChange();
    m_height = height;
}

void ConstraintView::setPlayWidth(double width)
{
    m_playWidth = width;
    update();
}

void ConstraintView::setValid(bool val)
{
    m_validConstraint = val;
}

void ConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit m_presenter.pressed(event->scenePos());
}

void ConstraintView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void ConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}

bool ConstraintView::warning() const
{
    return m_warning;
}

void ConstraintView::setWarning(bool warning)
{
    m_warning = warning;
}

