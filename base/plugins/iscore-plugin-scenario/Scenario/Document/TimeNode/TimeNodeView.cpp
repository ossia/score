#include <Process/Style/ScenarioStyle.hpp>
#include <qbrush.h>
#include <qgraphicssceneevent.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qpen.h>
#include <algorithm>

#include "Process/ModelMetadata.hpp"
#include "Scenario/Document/TimeNode/TimeNodeModel.hpp"
#include "Scenario/Document/VerticalExtent.hpp"
#include "TimeNodePresenter.hpp"
#include "TimeNodeView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

TimeNodeView::TimeNodeView(TimeNodePresenter& presenter,
                           QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);
    this->setZValue(1);
    this->setAcceptHoverEvents(true);
    this->setCursor(Qt::CrossCursor);

    m_color = presenter.model().metadata.color();
}

void TimeNodeView::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    QColor pen_color;
    if(isSelected())
    {
        pen_color = ScenarioStyle::instance().TimenodeSelected;
    }
    else
    {
        pen_color = m_color;
    }

    QPen pen{QBrush(pen_color), 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
    painter->setPen(pen);

    painter->fillRect(QRectF(QPointF(-1, 0), QPointF(1, m_extent.bottom() - m_extent.top())), QBrush(pen_color));

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::darkMagenta);
    painter->drawRect(boundingRect());
#endif
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
    if(event->button() == Qt::MouseButton::LeftButton)
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
