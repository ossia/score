#include "TimeNodeView.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QApplication>
#include "TimeNodePresenter.hpp"
TimeNodeView::TimeNodeView(TimeNodePresenter& presenter,
                           QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);
    this->setZValue(parent->zValue() + 1);
    this->setAcceptHoverEvents(true);
    this->setCursor(Qt::CrossCursor);

    // TODO in palette ?
    m_color = Qt::darkGray; // qApp->palette("ScenarioPalette").alternateBase().color();
}

void TimeNodeView::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    QColor pen_color = m_color;
    QColor highlight = QColor::fromRgbF(0.188235, 0.54902, 0.776471);

    if(isSelected())
    {
        pen_color = highlight;
    }
    if(! isValid())
    {
        pen_color = Qt::red;
    }

    QPen pen{QBrush(pen_color), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
    painter->setPen(pen);

    painter->drawRect(QRectF(QPointF(0, 0), QPointF(0, m_extent.bottom() - m_extent.top())));

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::darkMagenta);
    painter->drawRect(boundingRect());
#endif
}

QRectF TimeNodeView::boundingRect() const
{
    return { -3., 0., 6., m_extent.bottom() - m_extent.top()};
}

void TimeNodeView::setExtent(const VerticalExtent& extent)
{
    prepareGeometryChange();
    m_extent = extent;
    this->update();
}

void TimeNodeView::setExtent(VerticalExtent &&extent)
{
    prepareGeometryChange();
    m_extent = std::move(extent);
    this->update();
}

void TimeNodeView::setMoving(bool arg)
{
    m_moving = arg;
    update();
}

void TimeNodeView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void TimeNodeView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}


void TimeNodeView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.pressed(event->scenePos());
}

void TimeNodeView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void TimeNodeView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}
