#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/DefaultGraphicsKnobImpl.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsKnob);

namespace score
{

QGraphicsKnob::QGraphicsKnob(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsKnob::setRange(double min, double max)
{
  this->min = min;
  this->max = max;
  update();
}

void QGraphicsKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsKnob::value() const
{
  return m_value;
}

void QGraphicsKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsKnob::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 2),
      painter,
      widget);
}
}
