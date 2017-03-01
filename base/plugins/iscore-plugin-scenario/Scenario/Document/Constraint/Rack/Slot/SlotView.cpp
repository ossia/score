#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QCursor>
#include <QPen>
#include <qnamespace.h>

#include "SlotHandle.hpp"
#include "SlotOverlay.hpp"
#include "SlotView.hpp"


class QWidget;

namespace Scenario
{
SlotView::SlotView(const SlotPresenter& pres, QQuickPaintedItem* parent)
    : QQuickPaintedItem{parent}
    , presenter{pres}
    , m_handle{new SlotHandle{*this, this}}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setCursor(QCursor(Qt::ArrowCursor));
  this->setFlag(ItemClipsChildrenToShape, true);
  this->setZ(1);
  m_handle->setPosition(QPointF(
      0, this->boundingRect().height() - SlotHandle::handleHeight()));
  m_handle->setZ(100);
}

void SlotView::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  if (!m_focus)
    painter->setPen(ScenarioStyle::instance().ProcessViewBorder.getColor().color());
  else
    painter->setPen(ScenarioStyle::instance().SlotFocus.getColor().color());

  painter->drawLine(0, 0, width(), 0);
}

void SlotView::setHeight(qreal height)
{
  //prepareGeometryChange();
  QQuickPaintedItem::setHeight(height);
  m_handle->setPosition(
      QPointF(0, this->boundingRect().height() - SlotHandle::handleHeight()));
  if (m_overlay)
    m_overlay->setHeight(height);
}

void SlotView::setWidth(qreal width)
{
  QQuickPaintedItem::setWidth(width);
  if (m_overlay)
    m_overlay->setWidth(width);
  m_handle->setWidth(width);
}

void SlotView::enable()
{
  if (!m_overlay)
    return;

  this->update();

  for (QQuickItem* item : childItems())
  {
    item->setEnabled(true);
    //item->setFlag(ItemStacksBehindParent, false);
  }

  delete m_overlay;
  m_overlay = nullptr;

  this->update();
}

void SlotView::disable()
{
  delete m_overlay;

  for (QQuickItem* item : childItems())
  {
    item->setEnabled(false);
//    item->setFlag(ItemStacksBehindParent, true);
  }

  m_overlay = new SlotOverlay{this};
//  m_handle->setFlag(ItemStacksBehindParent, false);
  m_handle->setEnabled(true);
}

void SlotView::setFocus(bool b)
{
  m_focus = b;
  update();
}

void SlotView::setFrontProcessName(const QString& s)
{
  m_frontProcessName = s;
}
/*
void SlotView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), mapToScene(event->localPos()));
}
*/
}
