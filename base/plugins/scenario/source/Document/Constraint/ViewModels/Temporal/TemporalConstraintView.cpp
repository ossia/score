#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"
#include "TemporalConstraintPresenter.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

TemporalConstraintView::TemporalConstraintView(const TemporalConstraintPresenter &presenter, QGraphicsObject* parent) :
    AbstractConstraintView {presenter, parent}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 1);
}

QRectF TemporalConstraintView::boundingRect() const
{
    return {0, -18, qreal(maxWidth()), qreal(constraintHeight()) };
}

void TemporalConstraintView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QColor c = Qt::black;

    if(isSelected())
    {
        c = Qt::blue;
    }
    else if (shadow())
    {
        c = Qt::cyan;
    }

    if(defaultWidth() < 0)
    {
        c = Qt::red;
    }
    QPen playedPen{
        QBrush{Qt::green},
        4,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin
    };

    (m_moving ? c.setAlphaF(0.4) : c.setAlphaF(1.0));

    m_solidPen.setColor(c);
    m_dashPen.setColor(c);

    if(minWidth() == maxWidth())
    {
        painter->setPen(playedPen);
        painter->drawLine(0, 0, playDuration(), 0);
        painter->setPen(m_solidPen);
        painter->drawLine(playDuration(), 0, defaultWidth(), 0);
    }
    else if(infinite())
    {
        painter->setPen(m_solidPen);
        painter->drawLine(0,
                          0,
                          minWidth(),
                          0);

        QPen pen = m_dashPen;
        QLinearGradient gradient(minWidth(), 0, defaultWidth(), 0);
        gradient.setColorAt(0, c);
        gradient.setColorAt(1, Qt::transparent);


        pen.setColor(Qt::black);
        pen.setBrush(gradient);
        painter->setPen(pen);
        painter->drawLine(minWidth(),
                          0,
                          defaultWidth(),
                          0);
    }
    else
    {
        // Firs the line going from 0 to the min
        painter->setPen(m_solidPen);
        painter->drawLine(0,
                          0,
                          minWidth(),
                          0);

        // The little hat
        painter->drawLine(minWidth(),
                          -5,
                          minWidth(),
                          -15);
        painter->drawLine(minWidth(),
                          -15,
                          maxWidth(),
                          -15);
        painter->drawLine(maxWidth(),
                          -5,
                          maxWidth(),
                          -15);

        // Finally the dashed line
        painter->setPen(m_dashPen);
        painter->drawLine(minWidth(),
                          0,
                          maxWidth(),
                          0);
    }


}

void TemporalConstraintView::setMoving(bool arg)
{
    m_moving = arg;
    update();
}

void TemporalConstraintView::hoverEnterEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverEnterEvent(h);
    emit constraintHoverEnter();
}

void TemporalConstraintView::hoverLeaveEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverLeaveEvent(h);
    emit constraintHoverLeave();
}
bool TemporalConstraintView::shadow() const
{
    return m_shadow;
}

void TemporalConstraintView::setShadow(bool shadow)
{
    m_shadow = shadow;
    update();
}

