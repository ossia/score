#include "ProcessViewInterface.hpp"


QRectF ProcessViewInterface::boundingRect() const
{
    return {0, 0, m_width, m_height};
}


void ProcessViewInterface::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
}


qreal ProcessViewInterface::height() const
{
    return m_height;
}


void ProcessViewInterface::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
}


qreal ProcessViewInterface::width() const
{
    return m_width;
}
