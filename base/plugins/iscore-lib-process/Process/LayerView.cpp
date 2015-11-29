#include <Process/Style/ScenarioStyle.hpp>
#include <qpainter.h>

#include "LayerView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

LayerView::~LayerView()
{

}

QRectF LayerView::boundingRect() const
{
    return {0, 0, m_width, m_height};
}

void LayerView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    painter->setPen(ScenarioStyle::instance().ProcessViewBorder);
    painter->drawRect(boundingRect());

    paint_impl(painter);
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
