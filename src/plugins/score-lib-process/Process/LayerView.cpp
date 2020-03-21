// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LayerView.hpp"

#include <Process/Dataflow/CableItem.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <QGraphicsSceneDragDropEvent>
#include <QPainter>
#include <QStyleOption>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::LayerView)
namespace Process
{
HeaderDelegate::~HeaderDelegate() {}

LayerView::~LayerView()
{
  const auto& items = childItems();
  for (auto item : items)
  {
    if (item->type() == Dataflow::CableItem::static_type())
    {
      item->setParentItem(nullptr);
    }
  }
}
void LayerView::heightChanged(qreal) {}
void LayerView::widthChanged(qreal) {}

MiniLayer::~MiniLayer() = default;

LayerView::LayerView(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(ItemClipsChildrenToShape, false);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
}

void LayerView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if (auto p = parentItem())
    p->unsetCursor();
}
void LayerView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  if (auto p = parentItem())
    p->unsetCursor();
}

QRectF LayerView::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void LayerView::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  paint_impl(painter);
#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::green);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect());
#endif
}

void LayerView::setHeight(qreal height) noexcept
{
  if (height != m_height)
  {
    prepareGeometryChange();
    m_height = height;
    heightChanged(height);
  }
}

void LayerView::setWidth(qreal width) noexcept
{
  if (width != m_width)
  {
    prepareGeometryChange();
    m_width = width;
    widthChanged(width);
  }
}

QPixmap LayerView::pixmap() noexcept
{
  // Retrieve the bounding rect
  QRect rect = boundingRect().toRect();
  if (rect.isNull() || !rect.isValid())
  {
    return QPixmap();
  }

  // Create the pixmap
  QPixmap pixmap(rect.size());
  pixmap.fill(Qt::transparent);

  // Render
  QPainter painter(&pixmap);
  QStyleOptionGraphicsItem item;
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.translate(-rect.topLeft());
  paint(&painter, &item, nullptr);
  const auto& items = childItems();
  for (QGraphicsItem* child : items)
  {
    painter.save();
    painter.translate(child->mapToParent(pos()));
    child->paint(&painter, &item, nullptr);
    painter.restore();
  }

  painter.end();

  return pixmap;
}

void LayerView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::layerdata()))
  {
    event->accept();
  }
  else if (formats.contains(score::mime::processpreset()))
  {
    // TODO check if this is the right process
    event->accept();
    m_dropPresetOverlay = true;
    update();
  }
}

void LayerView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropPresetOverlay = false;
  update();
}

void LayerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::processpreset()))
  {
    event->accept();
    presetDropReceived(event->pos(), *event->mimeData());
    m_dropPresetOverlay = false;
    update();
  }
}

MiniLayer::MiniLayer(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(ItemClipsChildrenToShape, true);
}

QRectF MiniLayer::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void MiniLayer::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  paint_impl(painter);
}

void MiniLayer::setHeight(qreal height)
{
  prepareGeometryChange();
  m_height = height;
  update();
}

qreal MiniLayer::height() const
{
  return m_height;
}

void MiniLayer::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  update();
}

qreal MiniLayer::width() const
{
  return m_width;
}

void MiniLayer::setZoomRatio(qreal z)
{
  m_zoom = z;
  update();
}

qreal MiniLayer::zoom() const
{
  return m_zoom;
}
}


void Process::LayerView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  pressed(event->pos());
  event->accept();
}

void Process::LayerView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  moved(event->pos());
  event->accept();
}

void Process::LayerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  released(event->pos());
  event->accept();
}
