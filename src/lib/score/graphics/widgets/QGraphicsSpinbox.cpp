#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsSpinbox);

namespace score
{

QGraphicsSpinbox::QGraphicsSpinbox(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  min = -100;
  max = 100;
}

QGraphicsSpinbox::~QGraphicsSpinbox() = default;

void QGraphicsSpinbox::setValue(float v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void QGraphicsSpinbox::setExecutionValue(float v)
{
  m_execValue = ossia::clamp(v, 0., 1.);
  m_hasExec = true;
  update();
}

void QGraphicsSpinbox::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsSpinbox::setRange(float min, float max)
{
  this->min = min;
  this->max = max;
  update();
}

float QGraphicsSpinbox::value() const
{
  return m_value;
}

void QGraphicsSpinbox::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  m_value = 0.5;
  sliderMoved();
  sliderReleased();
  update();
}

void QGraphicsSpinbox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsSpinbox::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsSpinbox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

QRectF QGraphicsSpinbox::boundingRect() const
{
  return m_rect;
}

void QGraphicsSpinbox::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  const double val = map(m_value);
  auto text = score::toNumber(val);

  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);

  // Draw rect
  const QRectF brect = boundingRect();
  painter->drawRoundedRect(brect, 1, 1);

  // Draw text
  painter->setPen(skin.Base4.main.pen1);
  painter->setFont(skin.Medium8Pt);
  const auto textrect = brect.adjusted(2, 3, -2, -2);
  painter->drawText(textrect, text, QTextOption(Qt::AlignLeft));
}

}
