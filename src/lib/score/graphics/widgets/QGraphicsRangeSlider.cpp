#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsRangeSlider);

namespace score
{
QGraphicsRangeSlider::QGraphicsRangeSlider(QGraphicsItem* parent)
    : rangeRect{
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
    update();
  }
}

void QGraphicsRangeSlider::setEnd(double end)
{
  end = std::clamp(end, 0., 1.);
  if(m_end != end)
  {
    m_end = end;
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
  m_execValue[0] = ossia::clamp(v[0], 0., 1.);
  m_execValue[1] = ossia::clamp(v[1], 0., 1.);
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
}

void QGraphicsRangeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  switch(handle)
  {
    case START:
      d2s = event->pos().x() - m_start * m_rect.width();
      m_start = std::clamp(m_start + d2s / m_rect.width(), 0., 1.);
      break;
    case END:
      d2e = event->pos().x() - m_end * m_rect.width();
      m_end = std::clamp(m_end + d2e / m_rect.width(), 0., 1.);
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
  rangeRect
      = {m_rect.width() * m_start, m_rect.top(), m_rect.width() * (m_end - m_start),
         m_rect.height()};

  update();
}

void QGraphicsRangeSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{

  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->fillRect(boundingRect(), skin.Emphasis2.main.brush);
  painter->fillRect(rangeRect, skin.Base4.main.brush);

  painter->setRenderHint(QPainter::Antialiasing, false);
}

QRectF QGraphicsRangeSlider::boundingRect() const
{
  return m_rect;
}

}
