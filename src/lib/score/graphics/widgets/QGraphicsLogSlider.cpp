#include <score/graphics/widgets/QGraphicsLogSlider.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsLogSlider);

namespace score
{
template void QGraphicsSliderBase<QGraphicsLogSlider>::setRect(const QRectF& r);

QGraphicsLogSlider::QGraphicsLogSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogSlider::setRange(double min, double max)
{
  this->min = min;
  this->max = max;
  update();
}

void QGraphicsLogSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogSlider::value() const
{
  return m_value;
}

double QGraphicsLogSlider::map(double v) const noexcept
{
  return ossia::normalized_to_log(min, max - min, v);
}

double QGraphicsLogSlider::unmap(double v) const noexcept
{
  return ossia::log_to_normalized(min, max - min, v);
}

void QGraphicsLogSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsLogSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsLogSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsLogSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(ossia::normalized_to_log(min, max - min, value()), 'f', 3),
      painter,
      widget);
}
}
