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

ConditionView::ConditionView(QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_currentState{State::Waiting}
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
    QPen pen;
    switch(m_currentState)
    {
        case State::Waiting:
            pen = ScenarioStyle::instance().ConditionWaiting;
            break;
        case State::Disabled:
            pen = ScenarioStyle::instance().ConditionDisabled;
            break;
        case State::False:
            pen = ScenarioStyle::instance().ConditionFalse;
            break;
        case State::True:
            pen = ScenarioStyle::instance().ConditionTrue;
            break;
        default:
            pen = QColor(Qt::magenta);
            break;
    }

    pen.setWidth(2);
    painter->setPen(pen);
    painter->setBrush(Qt::transparent);
    painter->drawPath(*m_Cpath);

    pen.setWidth(1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(pen.color());
    painter->drawPath(m_trianglePath);
}

void ConditionView::changeHeight(qreal newH)
{
    prepareGeometryChange();
    m_height = newH;

    if(m_Cpath)
        delete m_Cpath;

    m_Cpath = new QPainterPath{};
    QRectF rect(boundingRect().topLeft(), QSize(m_CHeight,
                                                  m_CHeight));
    QRectF bottomRect(QPointF(boundingRect().bottomLeft().x(), boundingRect().bottomLeft().y() - m_CHeight),
                              QSize(m_CHeight,m_CHeight));

    m_Cpath->moveTo(boundingRect().width() / 2., 0);
    m_Cpath->arcTo(rect, 60, 120);
    m_Cpath->lineTo(0, m_height - m_CHeight/2);
    m_Cpath->arcTo(bottomRect, -180, 120);
    this->update();
}
