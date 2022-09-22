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

void QGraphicsRangeSlider::setValue(ossia::vec2f value)
{
  setStart(value[0]);
  setEnd(value[1]);
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
  return {float(m_start), float(m_end)};
}

void QGraphicsRangeSlider::setRange(double min, double max)
{
  m_min = min;
  m_max = max;
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
      handle = NONE;
  }
  event->accept();
}

void QGraphicsRangeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  switch(handle)
  {
    case START:
      d2s = event->pos().x() - m_start * m_rect.width();
      m_start = std::clamp(m_start + d2s / m_rect.width(), 0., m_end - 0.001);
      break;
    case END:
      d2e = event->pos().x() - m_end * m_rect.width();
      m_end = std::clamp(m_end + d2e / m_rect.width(), m_start + 0.001, 1.);
      break;
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
  sliderReleased();
  event->accept();
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
