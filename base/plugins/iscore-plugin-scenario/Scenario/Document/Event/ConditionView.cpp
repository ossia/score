#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QSize>
#include <QVector>

#include "ConditionView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
ConditionView::ConditionView(const QColor& color, QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_color{color}
{
    setFlag(ItemStacksBehindParent, true);

    changeHeight(0);

    m_trianglePath.addPolygon(QVector<QPointF>{
                                  QPointF(25, 5),
                                  QPointF(25, 21),
                                  QPointF(32, 14)
                              });
}

QRectF ConditionView::boundingRect() const
{
    return  QRectF{0, 0, m_width, m_height + m_CHeight};
}

void ConditionView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    QPen pen{m_color};

    pen.setWidth(2);
    painter->setPen(pen);
    painter->setBrush(Qt::transparent);
    painter->drawPath(m_Cpath);

#if !defined(ISCORE_IEEE_SKIN)
    pen.setWidth(1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(pen.color());
    painter->drawPath(m_trianglePath);
#endif
}

void ConditionView::changeHeight(qreal newH)
{
    prepareGeometryChange();
    m_height = newH;

    m_Cpath = QPainterPath();

    QRectF rect(boundingRect().topLeft(), QSize(m_CHeight,
                                                  m_CHeight));
    QRectF bottomRect(QPointF(boundingRect().bottomLeft().x(), boundingRect().bottomLeft().y() - m_CHeight),
                              QSize(m_CHeight,m_CHeight));

    m_Cpath.moveTo(boundingRect().width() / 2., 2);
    m_Cpath.arcTo(rect, 60, 120);
    m_Cpath.lineTo(0, m_height + m_CHeight/2);
    m_Cpath.arcTo(bottomRect, -180, 120);
    this->update();
}
}
