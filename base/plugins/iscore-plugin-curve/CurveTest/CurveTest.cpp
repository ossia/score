#include "CurveTest.hpp"
#include <QPainter>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

inline double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}

QRectF CurveSegmentView::boundingRect() const
{
    return rect;
}

void CurveSegmentView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::red);
    for(int i = 0; i < points.size() - 1; i++)
        painter->drawLine(points[i], points[i+1]);

    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}


QRectF CurveView::boundingRect() const
{
    return rect;
}


QPointF myscale(QPointF first, QSizeF second)
{
    return {first.x() * second.width(), first.y() * second.height()};
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}
#include <QDebug>
void CurveView::updateSubitems()
{

}

const id_type<CurveSegmentModel>& CurveSegmentModel::previous() const
{
    return m_previous;
}

void CurveSegmentModel::setPrevious(const id_type<CurveSegmentModel>& previous)
{
    if(previous != m_previous)
    {
        m_previous = previous;
        emit previousChanged();
    }
}
const id_type<CurveSegmentModel>& CurveSegmentModel::following() const
{
    return m_following;
}

void CurveSegmentModel::setFollowing(const id_type<CurveSegmentModel>& following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}
QPointF CurveSegmentModel::end() const
{
    return m_end;
}

QPointF CurveSegmentModel::start() const
{
    return m_start;
}


