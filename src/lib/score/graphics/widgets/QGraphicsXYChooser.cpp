#include <score/graphics/widgets/QGraphicsXYChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsXYChooser);

namespace score
{

QGraphicsXYChooser::QGraphicsXYChooser(QGraphicsItem* parent)
  : m_min{0.f, 0.f}
  , m_max{1.f, 1.f}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsXYChooser::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->fillRect(
      QRectF{0, 0, 100, 100}, score::Skin::instance().Dark.main.brush);

  auto [x, y] = m_value;
  x = 100. * (x - m_min[0]) / (m_max[0] - m_min[0]);
  y = 100. * (1. - ((y - m_min[1]) / (m_max[1] - m_min[1])));

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});
}

std::array<float, 2> QGraphicsXYChooser::value() const
{
  return m_value;
}

ossia::vec2f QGraphicsXYChooser::scaledValue(float x, float y) const noexcept
{
  return {
    m_min[0] + x * (m_max[0] - m_min[0]),
    m_min[1] + y * (m_max[1] - m_min[1])};
}

void QGraphicsXYChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  m_value = ossia::vec2f{0., 0.};
  m_grab = true;
  sliderMoved();
  sliderReleased();
  m_grab = false;
  update();
  event->accept();
}

void QGraphicsXYChooser::setValue(ossia::vec2f v)
{
  m_value = v;
  update();
}

void QGraphicsXYChooser::setRange(ossia::vec2f min, ossia::vec2f max)
{
  m_min = min;
  m_max = max;
}

void QGraphicsXYChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  float newX = qBound(0., p.x() / 100., 1.);
  float newY = qBound(0., 1. - (p.y() / 100.), 1.);
  m_grab = true;

  const ossia::vec2f newValue = scaledValue(newX, newY);
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
    float newY = qBound(0., 1. - (p.y() / 100.), 1.);
    m_grab = true;

    const ossia::vec2f newValue = scaledValue(newX, newY);
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
    float newY = qBound(0., 1. - (p.y() / 100.), 1.);
    m_grab = true;

    const ossia::vec2f newValue = scaledValue(newX, newY);
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
