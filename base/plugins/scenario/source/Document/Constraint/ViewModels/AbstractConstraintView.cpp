#include "AbstractConstraintView.hpp"

AbstractConstraintView::AbstractConstraintView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    setAcceptHoverEvents(true);
}

void AbstractConstraintView::setInfinite(bool infinite)
{
    prepareGeometryChange();

    m_infinite = infinite;
    update();
}

void AbstractConstraintView::setDefaultWidth(double width)
{
    prepareGeometryChange();
    m_defaultWidth = width;
}

void AbstractConstraintView::setMaxWidth(bool infinite, double max)
{
    prepareGeometryChange();

    setInfinite(infinite);
    if(!infinite)
    {
        //qDebug() << max;
        m_maxWidth = max;
    }

}

void AbstractConstraintView::setMinWidth(double min)
{
    prepareGeometryChange();
    m_minWidth = min;
}

void AbstractConstraintView::setHeight(double height)
{
    prepareGeometryChange();
    m_height = height;
}
