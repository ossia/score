#include <Process/Style/ScenarioStyle.hpp>
#include <QPainter>

#include "ProcessPanelGraphicsProxy.hpp"

namespace Process
{
class LayerModel;
}

class QWidget;

ProcessPanelGraphicsProxy::ProcessPanelGraphicsProxy()
{
//  this->setCacheMode(QQuickPaintedItem::NoCache);
}

QRectF ProcessPanelGraphicsProxy::boundingRect() const
{
  return {0, 0, m_size.width(), m_size.height()};
}

void ProcessPanelGraphicsProxy::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setBrush(
      ScenarioStyle::instance().ProcessPanelBackground.getBrush());
  auto rect = boundingRect();
  painter->setPen(Qt::DashLine);
  painter->drawLine(rect.width(), 0, rect.width(), rect.height());
}

void ProcessPanelGraphicsProxy::setRect(const QSizeF& size)
{
//  prepareGeometryChange();
  m_size = size;
}
