#include "CurveSegmentView.hpp"
#include "CurveSegmentModel.hpp"
#include <QPainter>

CurveSegmentView::CurveSegmentView(CurveSegmentModel *model, QGraphicsItem *parent):
    QGraphicsObject{parent},
    m_model{model}
{
    connect(m_model, &CurveSegmentModel::dataChanged,
            this, &CurveSegmentView::updatePoints);

    this->setZValue(1);
}

void CurveSegmentView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
    updatePoints();
}

QRectF CurveSegmentView::boundingRect() const
{
    return m_rect;
}

void CurveSegmentView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen;
    pen.setColor(Qt::red);
    pen.setWidth(3);
    painter->setPen(pen);
    for(int i = 0; i < m_points.size() - 1; i++)
        painter->drawLine(m_points[i], m_points[i+1]);

    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}


#include <QGraphicsSceneMouseEvent>
void CurveSegmentView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit pressed(event->scenePos());
}

void CurveSegmentView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit moved(event->scenePos());
}

void CurveSegmentView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit released(event->scenePos());
}

void CurveSegmentView::updatePoints()
{
    // Get the length of the segment to scale.
    double len = m_model->end().x() - m_model->start().x();
    double startx = m_model->start().x() * m_rect.width() / len;
    double scalex = m_rect.width() / len;

    auto pts = m_model->data(5); // Set the number of required points here.

    // Map to the scene coordinates
    std::transform(pts.begin(), pts.end(), pts.begin(),
                   [&] (const QPointF& pt) {
        return QPointF{
            pt.x() * scalex - startx,
            (1. - pt.y()) * m_rect.height()};
    });

    m_points = std::move(pts);
    update();
}
