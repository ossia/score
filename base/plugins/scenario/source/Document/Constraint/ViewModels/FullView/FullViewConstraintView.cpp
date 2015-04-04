#include "FullViewConstraintView.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "FullViewConstraintViewModel.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QWidget>
#include <QGraphicsProxyWidget>
#include <QPushButton>

FullViewConstraintView::FullViewConstraintView(QGraphicsObject* parent) :
    AbstractConstraintView {parent}
{
    this->setParentItem(parent);
    this->setFlag(ItemIsSelectable);

    this->setZValue(parent->zValue() + 1);
}

QRectF FullViewConstraintView::boundingRect() const
{
    return {0, -18, qreal(maxWidth()) + 3, qreal(constraintHeight()) + 3};
}

void FullViewConstraintView::paint(QPainter* painter,
                                   const QStyleOptionGraphicsItem* option,
                                   QWidget* widget)
{
    QColor c = Qt::black;

    if(isSelected())
    {
        c = Qt::blue;
    }
    else if(parentItem()->isSelected())
    {
        c = Qt::cyan;
    }

    m_solidPen.setColor(c);
    m_dashPen.setColor(c);

    if(minWidth() == maxWidth())
    {
        painter->setPen(m_solidPen);
        painter->drawLine(0,
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

    // TODO max -> +inf

    QLinearGradient gradient {qreal(maxWidth()), 0, qreal(maxWidth() + 200), 0};
    gradient.setColorAt(0, Qt::black);
    gradient.setColorAt(1, Qt::transparent);

    QBrush brush {gradient};
    painter->setBrush(brush);
    painter->setPen(QPen(brush, 4));

    painter->drawLine(maxWidth(), 0, maxWidth() + 200, 0);
}

void FullViewConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
    QGraphicsObject::mousePressEvent(m);

    m_clickedPoint = m->pos();
    emit constraintPressed();
}
