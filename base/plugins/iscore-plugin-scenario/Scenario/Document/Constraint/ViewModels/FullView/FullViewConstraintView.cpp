#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <QtGlobal>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <QCursor>

#include "FullViewConstraintPresenter.hpp"
#include "FullViewConstraintView.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
FullViewConstraintView::FullViewConstraintView(FullViewConstraintPresenter& presenter,
                                               QGraphicsItem *parent) :
    ConstraintView {presenter, parent}
{
    this->setParentItem(parent);
    this->setFlag(ItemIsSelectable);
    this->setCursor(Qt::CrossCursor);

    this->setZValue(1);
    this->setY(2*constraintHeight());
}

QRectF FullViewConstraintView::boundingRect() const
{
    return {0, -18, qreal(maxWidth()) + 3, qreal(constraintHeight()) + 3};
}

void FullViewConstraintView::paint(QPainter* painter,
                                   const QStyleOptionGraphicsItem* option,
                                   QWidget* widget)
{
    int min_w = static_cast<int>(minWidth());
    int max_w = static_cast<int>(maxWidth());
    int def_w = static_cast<int>(defaultWidth());

    QColor c;
    if(isSelected())
    {
        c = ScenarioStyle::instance().ConstraintSelected.getColor();
    }
    else if(parentItem()->isSelected())
    {
        c = ScenarioStyle::instance().ConstraintFullViewParentSelected.getColor();
    }
    else
    {
        c = ScenarioStyle::instance().ConstraintBase.getColor();
    }

    m_solidPen.setColor(c);
    m_dashPen.setColor(c);

    if(min_w == max_w)
    {
        painter->setPen(m_solidPen);
        painter->drawLine(0, 0, def_w, 0);
    }
    else
    {
        // Firs the line going from 0 to the min
        painter->setPen(m_solidPen);
        painter->drawLine(0, 0, min_w, 0);

        // The little hat
        painter->drawLine(min_w, -5, min_w, -15);
        painter->drawLine(min_w, -15, max_w, -15);
        painter->drawLine(max_w, -5, max_w, -15);

        // Finally the dashed line
        painter->setPen(m_dashPen);
        painter->drawLine(min_w, 0, max_w, 0);
    }
/*
    QLinearGradient gradient {qreal(max_w), 0, qreal(max_w + 200), 0};
    gradient.setColorAt(0, Qt::black);
    gradient.setColorAt(1, Qt::transparent);

    QBrush brush {gradient};
    painter->setBrush(brush);
    painter->setPen(QPen(brush, 4));

    painter->drawLine(max_w, 0, max_w + 200, 0);
    */
}
}
