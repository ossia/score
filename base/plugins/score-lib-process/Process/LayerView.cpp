// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <QPainter>
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>

#include "LayerView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;
namespace Process
{
LayerView::~LayerView() = default;
MiniLayer::~MiniLayer() = default;

LayerView::LayerView(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(ItemClipsChildrenToShape, true);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
}

void LayerView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if(auto p = parentItem())
    p->unsetCursor();
}
void LayerView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  if(auto p = parentItem())
    p->unsetCursor();
}

QRectF LayerView::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void LayerView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  paint_impl(painter);
#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::green);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect());
#endif
}

void LayerView::setHeight(qreal height)
{
  prepareGeometryChange();
  m_height = height;
  emit heightChanged();
}

qreal LayerView::height() const
{
  return m_height;
}

void LayerView::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  emit widthChanged();
}

qreal LayerView::width() const
{
  return m_width;
}

QPixmap LayerView::pixmap()
{
    // Retrieve the bounding rect
    QRect rect = boundingRect().toRect();
    if (rect.isNull() || !rect.isValid()) {
        return QPixmap();
    }

    // Create the pixmap
    QPixmap pixmap(rect.size());
    pixmap.fill(Qt::transparent);

    // Render
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(-rect.topLeft());
    paint(&painter, nullptr, nullptr);
    for (QGraphicsItem* child : childItems()) {
        painter.save();
        painter.translate(child->mapToParent(pos()));
        child->paint(&painter, nullptr, nullptr);
        painter.restore();
    }

    painter.end();

    return pixmap;
}

void LayerView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::layerdata()))
  {
    event->accept();
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
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
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
