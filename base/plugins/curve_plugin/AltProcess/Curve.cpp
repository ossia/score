#include "Curve.hpp"
#include <QDebug>
#include "CurvePoint.hpp"
#include "CurveSegment.hpp"
#include <QGraphicsSceneMouseEvent>

CurvePoint* Curve::makePoint(double x, double y)
{
    auto pt = new CurvePoint(this);
    pt->val = {x, y};
    return pt;
}

CurveSegment* Curve::makeSegment(CurvePoint* pt1, CurvePoint* pt2)
{
    auto seg = new LinearCurveSegment(this);
    seg->origin = pt1;
    seg->dest = pt2;

    return seg;
}

Curve::Curve(QList<QPointF> points, QGraphicsItem* parent):
    QGraphicsObject{parent}
{
    setZValue(parent->zValue() + 1);
    auto pt1 = makePoint(points[0].x(), points[0].y());
    addPoint(pt1);

    for(int i = 1; i < points.size(); i++)
    {
        auto pt2 = makePoint(points[i].x(), points[i].y());
        auto segt = makeSegment(pt1, pt2);
        addSegment(segt);
        addPoint(pt2);
        pt1 = pt2;
    }

    auto firstPt = static_cast<CurvePoint*>(m_curveObjects.first());
    firstPt->lockVertically();

    auto lastPt = static_cast<CurvePoint*>(m_curveObjects.last());
    lastPt->lockVertically();
}

void Curve::addPoint(CurvePoint* pt)
{
    pt->setParentItem(this);
    pt->setZValue(this->zValue() + 2);
    pt->setPos(pt->val.x() * m_width, pt->val.y() * m_height);

    // Algo pour refaire m_curveObjects;
    m_curveObjects.append(pt);
}

void Curve::addSegment(CurveSegment* segmt)
{
    segmt->setParentItem(this);
    segmt->setZValue(this->zValue() + 1);

    // Algo pour refaire m_curveObjects;
    m_curveObjects.append(segmt);
}

void Curve::updatePoint(CurvePoint* pt, double newX, double newY)
{
    using namespace std;
    auto pos = m_curveObjects.indexOf(pt);

    CurveSegment* s3left = pos > 1 ? static_cast<CurveSegment*>(m_curveObjects.at(pos - 1)) : nullptr;
    CurveSegment* s3right = pos < m_curveObjects.size() - 1 ? static_cast<CurveSegment*>(m_curveObjects.at(pos + 1)) : nullptr;

    if(s3left && s3left->origin->val.x() > newX) // Too much to the left
    {
        /***************************
         *              v
         *  0  1  2  3  4  5  6
         *  x ___ x ___ x ___ x
         *
         *        <-----
         *        v
         *  0  1  4  3  2  5  6
         *  x ___ x ___ x ___ x
         *
         **************************/
        // Swap the segments.
        auto p2 = s3left->origin;
        auto p2Index = m_curveObjects.indexOf(p2); // 2
        auto s1 = static_cast<CurveSegment*>(m_curveObjects.at(p2Index - 1)); // 1
        auto s5 = static_cast<CurveSegment*>(m_curveObjects.at(p2Index + 3)); // 5

        s1->dest = pt;
        s3left->origin = pt;
        s3left->dest = p2;
        s5->origin = s3left->dest;

        m_curveObjects[p2Index] = s3left->origin;
        m_curveObjects[p2Index + 2] = s3left->dest;
    }
    else if(s3right && s3right->dest->val.x() < newX) // Too much to the right
    {
        /***************************
         *        v
         *  0  1  2  3  4  5  6
         *  x ___ x ___ x ___ x
         *
         *        ----->
         *              v
         *  0  1  4  3  2  5  6
         *  x ___ x ___ x ___ x
         *
         **************************/

        auto p2Index = m_curveObjects.indexOf(s3right->origin); // 2
        auto s1 = static_cast<CurveSegment*>(m_curveObjects.at(p2Index - 1)); // 1
        auto s5 = static_cast<CurveSegment*>(m_curveObjects.at(p2Index + 3)); // 5

        auto p4 = s3right->dest;
        s1->dest = p4;
        s3right->origin = p4;
        s3right->dest = pt;
        s5->origin = pt;

        m_curveObjects[p2Index] = s3right->origin;
        m_curveObjects[p2Index + 2] = s3right->dest;
    }


    pt->val.setX(newX);
    pt->val.setY(newY);


    update();
}

QRectF Curve::boundingRect() const
{
    return {0, 0, m_width, m_height};
}

#include <QPainter>
void Curve::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    painter->setPen(Qt::transparent);
    painter->drawRect(boundingRect());

}

void Curve::setWidth(double d)
{
    m_width = d;
    prepareGeometryChange();
}

void Curve::setHeight(double d)
{
    m_height = d;
    prepareGeometryChange();
}

void Curve::setSize(QSize s)
{
    setWidth(s.width());
    setHeight(s.height());

    for(auto item : m_curveObjects)
    {
        if(CurvePoint* pt = dynamic_cast<CurvePoint*>(item))
        {
            pt->setPos(pt->val.x() * m_width, pt->val.y() * m_height);
        }
    }
}

void Curve::editingBegins(CurvePoint* pt)
{
    m_currentX = pt->val.x();
}

void Curve::editingFinished(CurvePoint* pt)
{
    emit pointMovingFinished(m_currentX, pt->val.x(), pt->val.y());
}

void Curve::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* evt)
{
    auto newpt = makePoint(evt->pos().x() / m_width, evt->pos().y() / m_height);

    for(auto& elt : m_curveObjects)
    {
        if(CurveSegment* segmt = dynamic_cast<CurveSegment*>(elt))
        {
            if(segmt->origin->val.x() < newpt->val.x()
                    && segmt->dest->val.x() > newpt->val.x())
            {
                auto pos = m_curveObjects.indexOf(segmt);
                delete segmt;
                m_curveObjects.removeAt(pos);

                auto segmtPrev = makeSegment(dynamic_cast<CurvePoint*>(m_curveObjects[pos - 1]), newpt);
                segmtPrev->setParentItem(this);
                auto segmtNext = makeSegment(newpt, dynamic_cast<CurvePoint*>(m_curveObjects[pos]));
                segmtNext->setParentItem(this);

                m_curveObjects.insert(pos, segmtPrev);
                m_curveObjects.insert(pos + 1, newpt);
                m_curveObjects.insert(pos + 2, segmtNext);

                break;
            }
        }
    }

    newpt->setParentItem(this);
    newpt->setPos(newpt->val.x() * m_width, newpt->val.y() * m_height);
    emit pointCreated(newpt->val);
}

void Curve::mousePressEvent(QGraphicsSceneMouseEvent* evt)
{
    emit mousePressed();
}
