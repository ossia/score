#include "AbstractConstraintView.hpp"

AbstractConstraintView::AbstractConstraintView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{

}

void AbstractConstraintView::setInfinite(bool infinite)
{
    prepareGeometryChange();

    m_infinite = infinite;
    update();
}

void AbstractConstraintView::setDefaultWidth(int width)
{
    prepareGeometryChange();
    m_defaultWidth = width;
}

void AbstractConstraintView::setMaxWidth(bool infinite, int max)
{
    prepareGeometryChange();

    setInfinite(infinite);
    if(!infinite)
    {
        qDebug() << max;
        m_maxWidth = max;
    }

}

void AbstractConstraintView::setMinWidth(int min)
{
    prepareGeometryChange();
    m_minWidth = min;
}

void AbstractConstraintView::setHeight(int height)
{
    prepareGeometryChange();
    m_height = height;
}
