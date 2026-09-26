#include <score/graphics/DefaultGraphicsSpinboxImpl.hpp>
#include <score/graphics/widgets/Stepper.hpp>

#include <cmath>
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

bool QGraphicsSpinbox::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    DefaultGraphicsSpinboxImpl::ungrabMouseEvent(*this, event);
  }
  return QGraphicsItem::sceneEvent(event);
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
  // Rounded: the value is stored normalized, and map() gives e.g. 100.99999
  // for 101 on a 0-100000 range, which truncation would turn into 100.
  return int(std::lround(map(m_value)));
}

void QGraphicsIntSpinbox::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  // Two quick clicks on the strip are two steps, not the text editor: Qt
  // sends the second press as a double click.
  if(score::Stepper::stepAt(m_rect, event->pos()) != 0)
  {
    mousePressEvent(event);
    return;
  }
  DefaultGraphicsSpinboxImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsIntSpinbox::step(int n)
{
  const int next = std::clamp(value() + n, int(std::ceil(min)), int(std::floor(max)));
  if(next == value())
    return;
  m_value = unmap(next);
  update();
  sliderMoved();
  sliderReleased();
}

void QGraphicsIntSpinbox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // The +/- strip swallows the press: no drag.
  if(event->button() == Qt::LeftButton)
  {
    if(const int step = score::Stepper::stepAt(m_rect, event->pos()); step != 0)
    {
      m_pressedStep = step;
      m_stepArmed = true;
      update();
      event->accept();
      return;
    }
  }
  DefaultGraphicsSpinboxImpl::mousePressEvent(*this, event);
}

void QGraphicsIntSpinbox::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_pressedStep != 0)
  {
    // Leaving un-presses it; the release is then a no-op.
    const bool armed = score::Stepper::stepAt(m_rect, event->pos()) == m_pressedStep;
    if(armed != m_stepArmed)
    {
      m_stepArmed = armed;
      update();
    }
    event->accept();
    return;
  }
  DefaultGraphicsSpinboxImpl::mouseMoveEvent(*this, event);
}

void QGraphicsIntSpinbox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    if(const int s = std::exchange(m_pressedStep, 0); s != 0)
    {
      update();
      if(std::exchange(m_stepArmed, false))
        step(s);
      event->accept();
      return;
    }
  }
  DefaultGraphicsSpinboxImpl::mouseReleaseEvent(*this, event);
}

bool QGraphicsIntSpinbox::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    // The release that would have cleared these is never coming.
    m_stepArmed = false;
    if(std::exchange(m_pressedStep, 0) != 0)
      update();
    DefaultGraphicsSpinboxImpl::ungrabMouseEvent(*this, event);
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF QGraphicsIntSpinbox::boundingRect() const
{
  return m_rect;
}

void QGraphicsIntSpinbox::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const int val = value();
  auto& skin = score::Skin::instance();
  if(!score::Stepper::visible(m_rect))
  {
    DefaultGraphicsSpinboxImpl::paint(*this, skin, score::toNumber(val), painter, widget);
    return;
  }

  // As DefaultGraphicsSpinboxImpl::paint, with the text left of the strip.
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);
  painter->drawRoundedRect(m_rect, 1, 1);

  painter->setPen(skin.Base4.main.pen1);
  painter->setFont(skin.Medium8Pt);
  const auto textrect = m_rect.adjusted(2, 3, -2 - score::Stepper::width, -2);
  painter->drawText(textrect, score::toNumber(val), QTextOption(Qt::AlignLeft));
  painter->setRenderHint(QPainter::Antialiasing, false);

  score::Stepper::paint(*painter, skin, m_rect, m_stepArmed ? m_pressedStep : 0);
}
}
