#include <score/graphics/widgets/QGraphicsLogKnob.hpp>
#include <score/graphics/DefaultGraphicsKnobImpl.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsLogKnob);

namespace score
{

QGraphicsLogKnob::QGraphicsLogKnob(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsLogKnob::setRange(double min, double max)
{
  this->min = min;
  this->max = max;
  update();
}

void QGraphicsLogKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogKnob::value() const
{
  return m_value;
}

double QGraphicsLogKnob::map(double v) const noexcept
{
  return ossia::normalized_to_log(min, max - min, v);
}

double QGraphicsLogKnob::unmap(double v) const noexcept
{
  return ossia::log_to_normalized(min, max - min, v);
}

void QGraphicsLogKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsLogKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogKnob::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsLogKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsLogKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsLogKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(ossia::normalized_to_log(min, max - min, value()), 'f', 3),
      painter,
      widget);
}
}
