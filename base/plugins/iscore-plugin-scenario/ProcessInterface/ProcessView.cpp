#include "ProcessView.hpp"


QRectF ProcessView::boundingRect() const
{
    return {0, 0, m_width, m_height};
}


void ProcessView::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
}


qreal ProcessView::height() const
{
    return m_height;
}


void ProcessView::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
}


qreal ProcessView::width() const
{
    return m_width;
}
