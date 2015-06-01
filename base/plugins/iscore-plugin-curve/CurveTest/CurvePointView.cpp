#include "CurvePointView.hpp"
#include <QPainter>
CurvePointView::CurvePointView(QGraphicsItem* parent):
    QGraphicsObject{parent}
{
    this->setZValue(2);
}

QRectF CurvePointView::boundingRect() const
{
    return {-3, -3, 6, 6};
}

void CurvePointView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen;
    pen.setColor(Qt::green);
    pen.setWidth(3);
    painter->setPen(pen);
    painter->setBrush(Qt::green);

    painter->drawEllipse(QPointF{0., 0.}, 3, 3);
}

const id_type<CurveSegmentModel>& CurvePointView::following() const
{
    return m_following;
}

void CurvePointView::setFollowing(const id_type<CurveSegmentModel> &following)
{
    m_following = following;
}

const id_type<CurveSegmentModel> &CurvePointView::previous() const
{
    return m_previous;
}

void CurvePointView::setPrevious(const id_type<CurveSegmentModel> &previous)
{
    m_previous = previous;
}

#include <QGraphicsSceneMouseEvent>
void CurvePointView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit pressed(event->scenePos());
}

void CurvePointView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit moved(event->scenePos());
}

void CurvePointView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit released(event->scenePos());
}
