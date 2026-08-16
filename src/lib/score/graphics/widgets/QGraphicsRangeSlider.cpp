#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

#include <algorithm>
#include <numeric>
W_OBJECT_IMPL(score::QGraphicsRangeSlider);

namespace score
{
QGraphicsRangeSlider::QGraphicsRangeSlider(QGraphicsItem* parent)
    : m_rangeRect{
        m_rect.width() * m_start, m_rect.top(), m_rect.width() * (m_end - m_start),
        m_rect.height()}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

QGraphicsRangeSlider::~QGraphicsRangeSlider()
{
  if(moving)
    sliderReleased();
}

void QGraphicsRangeSlider::setStart(double start)
{
  start = std::clamp(start, 0., 1.);
  if(m_start != start)
  {
    m_start = start;
    updateRect();
    update();
  }
}

void QGraphicsRangeSlider::setEnd(double end)
{
  end = std::clamp(end, 0., 1.);
  if(m_end != end)
  {
    m_end = end;
    updateRect();
    update();
  }
}

double QGraphicsRangeSlider::to01(double v) const noexcept
{
  const double range = m_max - m_min;
  return range > 0. ? (v - m_min) / range : 0.;
}

double QGraphicsRangeSlider::from01(double v) const noexcept
{
  return m_min + v * (m_max - m_min);
}

void QGraphicsRangeSlider::setValue(ossia::vec2f value)
{
  setStart(to01(value[0]));
  setEnd(to01(value[1]));
}

void QGraphicsRangeSlider::setExecutionValue(ossia::vec2f v)
{
  m_execValue[0] = std::clamp(v[0], 0.f, 1.f);
  m_execValue[1] = std::clamp(v[1], 0.f, 1.f);
  m_hasExec = true;
  update();
}

void QGraphicsRangeSlider::resetExecution()
{
  m_hasExec = false;
  update();
}

ossia::vec2f QGraphicsRangeSlider::value() const noexcept
{
  return {float(from01(m_start)), float(from01(m_end))};
}

void QGraphicsRangeSlider::setRange(double min, double max, ossia::vec2f init)
{
  if(max <= min)
    max = min + 1.;

  // Keep pointing at the same absolute value across a domain change
  const auto previous = value();

  m_min = min;
  m_max = max;
  m_init_start = init[0];
  m_init_end = init[1];

  setValue(previous);
  update();
}

void QGraphicsRangeSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  using namespace std;
  if(event->button() == Qt::LeftButton)
  {
    d2s = abs(event->pos().x() - m_start * m_rect.width());
    d2c = abs(event->pos().x() - (m_start + (m_end - m_start) / 2) * m_rect.width());
    d2e = abs(event->pos().x() - m_end * m_rect.width());

    if(d2s < d2c && d2s < d2e)
      handle = START;
    else if(d2e < d2c && d2e < d2s)
      handle = END;
    else if(d2c < d2s && d2c < d2e)
    {
      handle = CENTER;
      ypos = event->pos().y();
    }
    else
    {
      // No strict winner. This notably happens when the range is empty, where
      // the three handles sit on top of each other: leaving it at NONE made the
      // slider impossible to move at all. Grab whichever bound the click landed
      // on the side of.
      const double center = (m_start + (m_end - m_start) / 2) * m_rect.width();
      handle = (event->pos().x() < center) ? START : END;
    }
  }
  event->accept();
}

void QGraphicsRangeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  switch(handle)
  {
    case START: {
      // std::clamp is UB when lo > hi, which happens as soon as the range
      // collapses onto 0 (resp. 1 for the end handle).
      const double hi = std::max(0., m_end - 0.001);
      d2s = event->pos().x() - m_start * m_rect.width();
      m_start = std::clamp(m_start + d2s / m_rect.width(), 0., hi);
      break;
    }
    case END: {
      const double lo = std::min(1., m_start + 0.001);
      d2e = event->pos().x() - m_end * m_rect.width();
      m_end = std::clamp(m_end + d2e / m_rect.width(), lo, 1.);
      break;
    }
    case CENTER:
      d2c = event->pos().x() - (m_start + (m_end - m_start) / 2) * m_rect.width();
      ydiff = ypos - event->pos().y();
      ypos = event->pos().y();
      val1 = std::clamp(m_start + d2c / m_rect.width() - ydiff * y_factor, 0., 1.);
      val2 = std::clamp(m_end + d2c / m_rect.width() + ydiff * y_factor, 0., 1.);
      m_start = std::min(val1, val2);
      m_end = std::max(val1, val2);
      break;
    case NONE:
      break;
  }
  updateRect();
  sliderMoved();
  update();
  event->accept();
}

void QGraphicsRangeSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  mouseMoveEvent(event);
  handle = NONE;
  sliderReleased();
  event->accept();
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open. Guarded on `handle`, which this widget owns:
//! `moving` is written by whoever consumes the signals, so relying on it would
//! make the cleanup depend on the consumer having wired itself up correctly.
bool QGraphicsRangeSlider::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(handle != NONE)
    {
      handle = NONE;
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

void QGraphicsRangeSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();

  painter->fillRect(boundingRect(), skin.Emphasis2.main.brush);
  painter->fillRect(m_rangeRect, skin.Base4.main.brush);

  painter->setPen(skin.Base4.lighter.pen1);
  auto linesRect = m_rangeRect.adjusted(0, 1, 0, 0);
  painter->drawLine(linesRect.topLeft(), linesRect.bottomLeft());
  painter->drawLine(linesRect.topRight(), linesRect.bottomRight());
}

void QGraphicsRangeSlider::updateRect()
{
  m_rangeRect
      = {m_rect.width() * m_start, m_rect.top(), m_rect.width() * (m_end - m_start),
         m_rect.height()};
}

QRectF QGraphicsRangeSlider::boundingRect() const
{
  return m_rect;
}

}
