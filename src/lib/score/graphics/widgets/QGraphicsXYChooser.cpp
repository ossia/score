#include <score/graphics/widgets/QGraphicsXYChooser.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsXYChooser);

namespace score
{

QGraphicsXYChooser::QGraphicsXYChooser(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsXYChooser::setPoint(const QPointF& r)
{
  SCORE_TODO;
}

void QGraphicsXYChooser::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->fillRect(QRectF{0, 0, 100, 100}, score::Skin::instance().Dark.main.brush);

  auto x = m_value[0] * 100.;
  auto y = m_value[1] * 100.;

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});
}

std::array<float, 2> QGraphicsXYChooser::value() const
{
  return m_value;
}

void QGraphicsXYChooser::setValue(std::array<float, 2> v)
{
  m_value = v;
  update();
}

void QGraphicsXYChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  float newX = qBound(0., p.x() / 100., 1.);
  float newY = qBound(0., p.y() / 100., 1.);
  m_grab = true;

  ossia::vec2f newValue{newX, newY};
  if (m_value != newValue)
  {
    m_value = newValue;
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsXYChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    float newX = qBound(0., p.x() / 100., 1.);
    float newY = qBound(0., p.y() / 100., 1.);
    m_grab = true;

    ossia::vec2f newValue{newX, newY};
    if (m_value != newValue)
    {
      m_value = newValue;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsXYChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    float newX = qBound(0., p.x() / 100., 1.);
    float newY = qBound(0., p.y() / 100., 1.);
    m_grab = true;

    ossia::vec2f newValue{newX, newY};
    if (m_value != newValue)
    {
      m_value = newValue;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsXYChooser::boundingRect() const
{
  return QRectF{0, 0, 100, 100};
}
}
