#include <score/graphics/widgets/QGraphicsIntSlider.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsIntSlider);

namespace score
{
template void QGraphicsSliderBase<QGraphicsIntSlider>::setRect(const QRectF& r);

QGraphicsIntSlider::QGraphicsIntSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsIntSlider::setValue(int v)
{
  m_value = ossia::clamp(v, m_min, m_max);
  update();
}

void QGraphicsIntSlider::setRange(int min, int max)
{
  m_min = min;
  m_max = max;
  update();
}

int QGraphicsIntSlider::value() const
{
  return m_value;
}

void QGraphicsIntSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(m_min + curPos * (m_max - m_min));
  if (res != m_value)
  {
    m_value = res;
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsIntSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsIntSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

void QGraphicsIntSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::number(value()), painter, widget);
}

double QGraphicsIntSlider::getHandleX() const
{
  return sliderRect().width() * ((double(m_value) - m_min) / (m_max - m_min));
}

}
