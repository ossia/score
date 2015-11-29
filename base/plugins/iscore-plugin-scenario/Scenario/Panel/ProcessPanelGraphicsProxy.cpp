#include <Process/Style/ScenarioStyle.hpp>
#include <qpainter.h>

#include "ProcessPanelGraphicsProxy.hpp"

class LayerModel;
class QStyleOptionGraphicsItem;
class QWidget;

ProcessPanelGraphicsProxy::ProcessPanelGraphicsProxy(const LayerModel& lm,
                                                     const ProcessPanelPresenter& pres)
{
}

QRectF ProcessPanelGraphicsProxy::boundingRect() const
{
    return {0, 0, m_size.width(), m_size.height()};
}

void ProcessPanelGraphicsProxy::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget)
{
    painter->setBrush(ScenarioStyle::instance().ProcessPanelBackground);
    painter->drawRect(boundingRect());
}

void ProcessPanelGraphicsProxy::setSize(const QSizeF& size)
{
    prepareGeometryChange();
    m_size = size;
}
