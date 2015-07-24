#include "LayerView.hpp"


QRectF LayerView::boundingRect() const
{
    return {0, 0, m_width, m_height};
}


void LayerView::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
}


qreal LayerView::height() const
{
    return m_height;
}


void LayerView::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
}


qreal LayerView::width() const
{
    return m_width;
}
