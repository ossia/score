#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsRangeSlider);

namespace score
{
QGraphicsRangeSlider::QGraphicsRangeSlider(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsRangeSlider::setStart(double start)
{
  if (m_start != start)
  {
    m_start = std::clamp(start, 0., 1.);
    update();
  }
}

void QGraphicsRangeSlider::setEnd(double end)
{
  if (m_end != end)
  {
    m_end = std::clamp(end, 0., 1.);
    update();
  }
}

void QGraphicsRangeSlider::setValue(ossia::vec2f value)
{
  setStart(value[0]);
  setEnd(value[1]);
}

ossia::vec2f QGraphicsRangeSlider::value() const noexcept
{
  return {float(m_start), float(m_end)};
}

void QGraphicsRangeSlider::setRange(double min, double max)
{
  {
    m_min = min;
    m_max = max;
    update();
  }
}

void QGraphicsRangeSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    d2s = abs(event->pos().x() - m_start * boundingRect().width());
    d2c = abs(
        event->pos().x()
        - (m_start + (m_end - m_start) / 2) * boundingRect().width());
    d2e = abs(event->pos().x() - m_end * boundingRect().width());

    if (d2s < d2c && d2s < d2e)
      handle = START;
    else if (d2e < d2c && d2e < d2s)
      handle = END;
    else if (d2c < d2s && d2c < d2e)
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
  switch (handle)
  {
    case START:
      d2s = event->pos().x() - m_start * boundingRect().width();
      m_start = std::clamp(m_start + d2s / boundingRect().width(), 0., 1.);
      break;
    case END:
      d2e = event->pos().x() - m_end * boundingRect().width();
      m_end = std::clamp(m_end + d2e / boundingRect().width(), 0., 1.);
      break;
    case CENTER:
      d2c = event->pos().x()
            - (m_start + (m_end - m_start) / 2) * boundingRect().width();
      ydiff = ypos - event->pos().y();
      ypos = event->pos().y();
      val1 = std::clamp(
          m_start + d2c / boundingRect().width() - ydiff * y_factor, 0., 1.);
      val2 = std::clamp(
          m_end + d2c / boundingRect().width() + ydiff * y_factor, 0., 1.);
      m_start = std::min(val1, val2);
      m_end = std::max(val1, val2);
      break;
    case NONE:
      break;
  }
  rangeRect
      = {boundingRect().width() * m_start,
         boundingRect().top(),
         boundingRect().width() * (m_end - m_start),
         boundingRect().height()};

  update();
}

void QGraphicsRangeSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{

  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->fillRect(boundingRect(), skin.Emphasis2.main.brush);
  painter->fillRect(
      rangeRect,
      skin.Emphasis2.lighter180.brush); ///TODO: use right brush from skin here

  painter->setRenderHint(QPainter::Antialiasing, false);
}

QRectF QGraphicsRangeSlider::boundingRect() const
{
  return {m_rect};
}

}
