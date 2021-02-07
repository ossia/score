#include <score/graphics/widgets/QGraphicsSlider.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsSlider);

namespace score
{
template void QGraphicsSliderBase<QGraphicsSlider>::setRect(const QRectF& r);

QGraphicsSlider::QGraphicsSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsSlider::setRange(double min, double max)
{
  this->min = min;
  this->max = max;
  update();
}

void QGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsSlider::value() const
{
  return m_value;
}

void QGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 3),
      painter,
      widget);
}
}
