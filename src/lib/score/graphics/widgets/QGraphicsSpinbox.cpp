#include <score/graphics/DefaultGraphicsSpinboxImpl.hpp>
#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsSpinbox);
W_OBJECT_IMPL(score::QGraphicsIntSpinbox);

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

QGraphicsSpinbox::~QGraphicsSpinbox()
{
  if(m_grab)
    sliderReleased();
}

void QGraphicsSpinbox::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void QGraphicsSpinbox::setExecutionValue(double v)
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

void QGraphicsSpinbox::setRange(double min, double max, double init)
{
  this->min = min;
  this->max = max;
  this->init = init;
  update();
}

void QGraphicsSpinbox::setNoValueChangeOnMove(bool b)
{
  m_noValueChangeOnMove = b;
}

double QGraphicsSpinbox::value() const
{
  return m_value;
}

void QGraphicsSpinbox::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsSpinbox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mousePressEvent(*this, event);
}

void QGraphicsSpinbox::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseMoveEvent(*this, event);
}

void QGraphicsSpinbox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseReleaseEvent(*this, event);
}

QRectF QGraphicsSpinbox::boundingRect() const
{
  return m_rect;
}

void QGraphicsSpinbox::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const double val = map(m_value);

  DefaultGraphicsSpinboxImpl::paint(
      *this, score::Skin::instance(), score::toNumber(val), painter, widget);
}

QGraphicsIntSpinbox::QGraphicsIntSpinbox(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  min = -100;
  max = 100;
}

QGraphicsIntSpinbox::~QGraphicsIntSpinbox() = default;

void QGraphicsIntSpinbox::setValue(double v)
{
  m_value = unmap(v);
  update();
}

void QGraphicsIntSpinbox::setExecutionValue(double v)
{
  m_execValue = ossia::clamp(v, min, max);
  m_hasExec = true;
  update();
}

void QGraphicsIntSpinbox::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsIntSpinbox::setRange(double min, double max, double init)
{
  this->min = min;
  this->max = max;
  this->init = init;
  update();
}

void QGraphicsIntSpinbox::setNoValueChangeOnMove(bool b)
{
  m_noValueChangeOnMove = b;
}

int QGraphicsIntSpinbox::value() const
{
  return map(m_value);
}

void QGraphicsIntSpinbox::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsIntSpinbox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mousePressEvent(*this, event);
}

void QGraphicsIntSpinbox::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseMoveEvent(*this, event);
}

void QGraphicsIntSpinbox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSpinboxImpl::mouseReleaseEvent(*this, event);
}

QRectF QGraphicsIntSpinbox::boundingRect() const
{
  return m_rect;
}

void QGraphicsIntSpinbox::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const int val = map(m_value);

  DefaultGraphicsSpinboxImpl::paint(
      *this, score::Skin::instance(), score::toNumber(val), painter, widget);
}
}
