#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QPainter>

#include "SlotOverlay.hpp"
#include "SlotPresenter.hpp"
#include "SlotView.hpp"


class QWidget;

namespace Scenario
{
SlotOverlay::SlotOverlay(SlotView* parent)
    : GraphicsItem{parent}, m_slotView{*parent}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setZ(1500);
}

QRectF SlotOverlay::boundingRect() const
{
  const auto& rect = m_slotView.boundingRect();
  return {0, 0, rect.width(), rect.height() - 5};
}

void SlotOverlay::setHeight(qreal height)
{
  //prepareGeometryChange();
}
void SlotOverlay::setWidth(qreal width)
{
  //prepareGeometryChange();
}

void SlotOverlay::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setPen(ScenarioStyle::instance().SlotOverlayBorder.getColor());
  painter->setBrush(ScenarioStyle::instance().SlotOverlay.getColor());
  painter->drawRect(boundingRect());
}

void SlotOverlay::mousePressEvent(QMouseEvent* event)
{
  emit m_slotView.presenter.pressed(mapToScene(event->localPos()));
}

void SlotOverlay::mouseMoveEvent(QMouseEvent* event)
{
  emit m_slotView.presenter.moved(mapToScene(event->localPos()));
}

void SlotOverlay::mouseReleaseEvent(QMouseEvent* event)
{
  emit m_slotView.presenter.released(mapToScene(event->localPos()));
}
}
