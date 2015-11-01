#include "ConditionView.hpp"
#include <QPainter>
#include <Process/Style/ScenarioStyle.hpp>
ConditionView::ConditionView(QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_currentState{State::Waiting}
{
    setFlag(ItemStacksBehindParent, true);
    QRectF square(boundingRect().topLeft(), QSize(boundingRect().height(),
                                                  boundingRect().height()));

    m_Cpath.moveTo(boundingRect().height() / 2., 0);
    m_Cpath.arcTo(square, 60, 230);

    m_trianglePath.addPolygon(QVector<QPointF>{
                                  QPointF(25, 5),
                                  QPointF(25, 21),
                                  QPointF(32, 14)
                              });
}

QRectF ConditionView::boundingRect() const
{
    return  QRectF{0, 0, 40, 27};
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
    painter->drawPath(m_Cpath);

    pen.setWidth(1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(pen.color());
    painter->drawPath(m_trianglePath);
}
