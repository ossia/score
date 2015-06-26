#include "ProcessPanelGraphicsProxy.hpp"
#include <ProcessInterface/ProcessModel.hpp>
#include "ProcessPanelPresenter.hpp"
#include <QPainter>

ProcessPanelGraphicsProxy::ProcessPanelGraphicsProxy(const LayerModel& lm,
                                                     const ProcessPanelPresenter& pres)
{
    QColor col = Qt::darkGray;
    col.setAlphaF(0.7);
    m_bgBrush = col;

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
    painter->setBrush(QColor::fromRgba(qRgba(0, 127, 229, 76)));
    painter->drawRect(boundingRect());
}

void ProcessPanelGraphicsProxy::setSize(const QSizeF& size)
{
    prepareGeometryChange();
    m_size = size;
}
