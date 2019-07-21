#include <score/graphics/RectItem.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneEvent>
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

void RectItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  static const auto pen = QPen{QColor(qRgba(80, 100, 140, 100)),
                               2,
                               Qt::SolidLine,
                               Qt::RoundCap,
                               Qt::RoundJoin};
  static const auto highlight_pen = QPen{QColor(qRgba(100, 120, 180, 100)),
                                         2,
                                         Qt::SolidLine,
                                         Qt::RoundCap,
                                         Qt::RoundJoin};
  static const auto brush = QBrush{Qt::transparent};

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(brush);
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

ResizeableItem::ResizeableItem(QGraphicsItem* parent)
  : QGraphicsItem{parent}
{

}

ResizeableItem::~ResizeableItem()
{

}

EmptyRectItem::EmptyRectItem(QGraphicsItem* parent)
  : ResizeableItem{parent}
{
  this->setFlag(ItemHasNoContents, true);
}
void EmptyRectItem::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  sizeChanged({r.width(), r.height()});
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
  event->accept();
}
void EmptyRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void EmptyRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  clicked();
  event->accept();
}

void BackgroundItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(style.NoPen);
  painter->setBrush(style.Transparent3);
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


void BackgroundItem::mousePressEvent(QGraphicsSceneMouseEvent* event) { event->accept(); }


void BackgroundItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) { event->accept(); }


void BackgroundItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) { event->accept(); }

}
