#include "Layer.hpp"


QRectF Layer::boundingRect() const
{
    return {0, 0, m_width, m_height};
}


void Layer::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
}


qreal Layer::height() const
{
    return m_height;
}


void Layer::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
}


qreal Layer::width() const
{
    return m_width;
}
