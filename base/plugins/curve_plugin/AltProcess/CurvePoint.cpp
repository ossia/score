#include "CurvePoint.hpp"
#include "Curve.hpp"
#include <QPainter>
#include <QDebug>

CurvePoint::CurvePoint(Curve* parent):
    CurveItem{parent},
    m_curve{parent}
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
}

QRectF CurvePoint::boundingRect() const
{
    return {-6, -6, 12, 12};
}

void CurvePoint::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if(m_hover)
    {
        painter->setPen(QPen(Qt::red, 3));
        painter->setBrush(Qt::red);
        painter->drawEllipse({0, 0}, 5, 5);

        painter->setPen(QColor(255, 80, 80));
        painter->setBrush(QColor(255, 200, 200));
        painter->drawEllipse({0, 0}, 3, 3);
    }
    else
    {
        painter->setPen(Qt::red);
        painter->setBrush(Qt::red);
        painter->drawEllipse({0, 0}, 3, 3);
    }
}

double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}

QVariant CurvePoint::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange)
    {
        auto newPt = value.toPointF();

        double newX;
        if(!m_locked)
        {
            newX = clamp(newPt.x() / m_curve->width(), 0.0, 1.0);
        }
        else
        {
            newX = val.x();
        }


        auto newPtY = clamp(newPt.y(), 0.0, m_curve->height());
        double newY = newPtY / m_curve->height();

        m_curve->updatePoint(this, newX, newY);

        return QPointF(newX * m_curve->width(),
                       newY * m_curve->height());
    }

    return QGraphicsItem::itemChange(change, value);
}

void CurvePoint::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_curve->editingBegins(this);
    QGraphicsItem::mousePressEvent(event);
}

void CurvePoint::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_curve->editingFinished(this);
    QGraphicsItem::mouseReleaseEvent(event);
}

void CurvePoint::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    m_hover = true;
    update();
}

void CurvePoint::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_hover = false;
    update();
}
