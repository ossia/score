#include <score/graphics/RectItem.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::RectItem)
W_OBJECT_IMPL(score::ResizeableItem)
W_OBJECT_IMPL(score::EmptyRectItem)
namespace score
{

void RectItem::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  sizeChanged({r.width(), r.height()});
}

void RectItem::setHighlight(bool b)
{
  m_highlight = b;
  update();
}

QRectF RectItem::boundingRect() const
{
  return m_rect;
}

void RectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();

  const auto& nobrush = skin.NoBrush;
  const auto& brush = skin.Emphasis5;

  const auto& pen = brush.main.pen2_solid_round_round;
  const auto& highlight_pen = brush.lighter.pen2_solid_round_round;

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(nobrush);
  painter->setPen(!m_highlight ? pen : highlight_pen);
  painter->drawRoundedRect(m_rect, 5, 5);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void RectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(10);
}

void RectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(0);
}
void RectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void RectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void RectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  clicked();
  event->accept();
}

ResizeableItem::ResizeableItem(QGraphicsItem* parent) : QGraphicsItem{parent} { }

ResizeableItem::~ResizeableItem() { }

EmptyRectItem::EmptyRectItem(QGraphicsItem* parent) : ResizeableItem{parent}
{
  this->setFlag(ItemHasNoContents, true);
  this->setAcceptedMouseButtons({});
}
void EmptyRectItem::setRect(const QRectF& r)
{
  if (r != m_rect)
  {
    prepareGeometryChange();
    m_rect = r;
    sizeChanged({r.width(), r.height()});
    update();
  }
}

QRectF EmptyRectItem::boundingRect() const
{
  return m_rect;
}

void EmptyRectItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  // painter->setPen(Qt::blue);
  // painter->setBrush(Qt::transparent);
  // painter->drawRect(boundingRect());
}

void EmptyRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(10);
}

void EmptyRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(0);
}
void EmptyRectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->ignore();
}
void EmptyRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->ignore();
}
void EmptyRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  //clicked();
  event->ignore();
}

BackgroundItem::BackgroundItem(QGraphicsItem *parent)
    : QGraphicsItem{parent}
{
  setAcceptedMouseButtons({});
}

BackgroundItem::~BackgroundItem()
{

}

void BackgroundItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& style = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(style.NoPen);
  painter->setBrush(style.Background2);
  painter->drawRoundedRect(m_rect.adjusted(2., 2., -2., -2.), 3, 3);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void BackgroundItem::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  update();
}

QRectF BackgroundItem::boundingRect() const
{
  return m_rect;
}

void BackgroundItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  QGraphicsItem::mousePressEvent(event);
  event->ignore();
}

void BackgroundItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  QGraphicsItem::mouseMoveEvent(event);
  event->ignore();
}

void BackgroundItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  QGraphicsItem::mouseReleaseEvent(event);
  event->ignore();
}

EmptyItem::EmptyItem(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  setFlag(ItemHasNoContents, true);
}

QRectF EmptyItem::boundingRect() const
{
  return {};
}

void EmptyItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

}
