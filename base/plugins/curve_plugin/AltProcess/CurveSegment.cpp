#include "CurveSegment.hpp"
#include "CurvePoint.hpp"
#include "Curve.hpp"
#include <QPainter>
#include <QDebug>

CurveSegment::CurveSegment(Curve* parent):
    CurveItem{parent}
{
    setAcceptHoverEvents(true);
}

QRectF CurveSegment::boundingRect() const
{
    return QRectF{origin->pos().x(),
                  origin->pos().y(),
                  std::abs(dest->pos().x() - origin->pos().x()),
                  std::abs(dest->pos().y() - origin->pos().y())};
}

bool CurveSegment::hovering() const
{
    return m_hover;
}

void CurveSegment::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    m_hover = true;
    update();
}

void CurveSegment::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_hover = false;
    update();
}


///////////////////
/*
QPainterPath LinearCurveSegment::shape() const
{
    QPainterPath path;
    QPainterPathStroker stroker;
    stroker.setWidth(2);
    path.lineTo(origin->pos());
    path.lineTo(dest->pos());

    return stroker.createStroke(path);
}*/

void LinearCurveSegment::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
 //   if(!hovering())
 //       painter->setPen(QPen(Qt::black, 2));
 //   else
        painter->setPen(QPen(Qt::black, 2));

    painter->drawLine(origin->pos(), dest->pos());
}
