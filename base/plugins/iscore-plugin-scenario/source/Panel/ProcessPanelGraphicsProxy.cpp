#include "ProcessPanelGraphicsProxy.hpp"
#include <ProcessInterface/ProcessModel.hpp>
#include "ProcessPanelPresenter.hpp"
#include <QPainter>
ProcessPanelGraphicsProxy::ProcessPanelGraphicsProxy(const ProcessViewModel& pvm,
                                                     const ProcessPanelPresenter& pres):
    m_pvm{pvm},
    m_pres{pres}
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

    //painter->fillRect(rect, qApp->palette("ScenarioPalette").background());
    painter->setBrush(Qt::darkGray);
    painter->drawRect(boundingRect());

    painter->setBrush(Qt::red);
    auto processWidth = m_pvm.sharedProcessModel().duration().msec() / m_pres.zoomRatio();
    painter->drawRect(processWidth, 0, boundingRect().width() - processWidth, boundingRect().height());
}

void ProcessPanelGraphicsProxy::setSize(const QSizeF& size)
{
    prepareGeometryChange();
    m_size = size;
}
