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

void ProcessPanelGraphicsProxy::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setBrush(
      ScenarioStyle::instance().ProcessPanelBackground.getColor());
  auto rect = boundingRect();
  painter->setPen(Qt::DashLine);
  painter->drawLine(rect.width(), 0, rect.width(), rect.height());
}

void ProcessPanelGraphicsProxy::setRect(const QSizeF& size)
{
  setWidth(size.width());
  setHeight(size.height());
}
