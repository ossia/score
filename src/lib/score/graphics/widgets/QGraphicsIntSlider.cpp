#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/widgets/QGraphicsIntSlider.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/StringConstants.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsIntSlider);

namespace score
{
template void QGraphicsSliderBase<QGraphicsIntSlider>::setRect(const QRectF& r);

QGraphicsIntSlider::QGraphicsIntSlider(QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

double QGraphicsIntSlider::from01(double v) const noexcept
{
  return v * (max - min) + min;
}

double QGraphicsIntSlider::map(double v) const noexcept
{
  return v;
}

double QGraphicsIntSlider::unmap(double v) const noexcept
{
  return v;
}

void QGraphicsIntSlider::setValue(int v)
{
  m_value = ossia::clamp(v, min, max);
  update();
}

void QGraphicsIntSlider::setExecutionValue(int v)
{
  m_execValue = ossia::clamp(v, min, max);
  m_hasExec = true;
  update();
}

void QGraphicsIntSlider::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsIntSlider::setRange(int min, int max)
{
  this->min = min;
  this->max = max;
  update();
}

int QGraphicsIntSlider::value() const
{
  return m_value;
}

void QGraphicsIntSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsIntSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsIntSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsIntSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), score::toNumber(value()), painter, widget);
}

double QGraphicsIntSlider::getHandleX() const
{
  if(max != min)
    return sliderRect().width() * ((double(m_value) - min) / (max - min));
  return 0;
}

double QGraphicsIntSlider::getExecHandleX() const
{
  if(max != min)
    return sliderRect().width() * ((double(m_execValue) - min) / (max - min));
  return 0;
}

}
