#include <QtGlobal>
#include <QGraphicsSceneEvent>

#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.hpp>

namespace Scenario
{
ConstraintView::ConstraintView(
        ConstraintPresenter& presenter,
        QGraphicsItem *parent) :
    QGraphicsItem{parent},
    m_presenter{presenter}
{
    setAcceptHoverEvents(true);
    m_leftBrace = new LeftBraceView{*this, this};
    m_leftBrace->setX(minWidth());
    m_leftBrace->hide();

    m_rightBrace = new RightBraceView{*this, this};
    m_rightBrace->setX(maxWidth());
    m_rightBrace->hide();
}

ConstraintView::~ConstraintView() = default;

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
    update();
}

void ConstraintView::setMaxWidth(bool infinite, double max)
{
    prepareGeometryChange();

    setInfinite(infinite);
    if(!infinite)
    {
        m_maxWidth = max;
    }
    update();
}

void ConstraintView::setMinWidth(double min)
{
    prepareGeometryChange();
    m_minWidth = min;
    update();
}

void ConstraintView::setHeight(double height)
{
    prepareGeometryChange();
    m_height = height;
    update();
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
}

