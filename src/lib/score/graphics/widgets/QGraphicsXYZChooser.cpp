#include <score/graphics/widgets/QGraphicsXYZChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsXYZChooser);
W_OBJECT_IMPL(score::QGraphicsXYZSpinboxChooser);

namespace score
{

QGraphicsXYZChooser::QGraphicsXYZChooser(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}
QGraphicsXYZChooser::~QGraphicsXYZChooser() = default;

void QGraphicsXYZChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->fillRect(QRectF{0, 0, 100, 100}, score::Skin::instance().Dark.main.brush);
  painter->fillRect(QRectF{110, 0, 20, 100}, score::Skin::instance().Dark.main.brush);

  auto [x, y, z] = m_value;
  x = 100. * (x - m_min[0]) / (m_max[0] - m_min[0]);
  y = 100. * (1. - ((y - m_min[1]) / (m_max[1] - m_min[1])));
  z = 100. * (1. - ((z - m_min[2]) / (m_max[2] - m_min[2])));

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0., y}, QPointF{100., y});
  painter->drawLine(QPointF{110., z}, QPointF{130., z});
}

std::array<float, 3> QGraphicsXYZChooser::value() const
{
  return m_value;
}

ossia::vec3f QGraphicsXYZChooser::scaledValue(float x, float y, float z) const noexcept
{
  return {
      m_min[0] + x * (m_max[0] - m_min[0]), m_min[1] + y * (m_max[1] - m_min[1]),
      m_min[2] + z * (m_max[2] - m_min[2])};
}

void QGraphicsXYZChooser::setValue(ossia::vec3f v)
{
  m_value = v;
  update();
}

void QGraphicsXYZChooser::setRange(ossia::vec3f min, ossia::vec3f max)
{
  m_min = min;
  m_max = max;
  prev_v[0] = (m_value[0] - m_min[0]) / (m_max[0] - m_min[0]);
  prev_v[1] = (m_value[1] - m_min[2]) / (m_max[1] - m_min[1]);
  prev_v[2] = (m_value[2] - m_min[2]) / (m_max[2] - m_min[2]);
}

void QGraphicsXYZChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  if(p.x() < 100.)
  {
    prev_v[0] = qBound(0., p.x() / 100., 1.);
    prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
  }
  else if(p.x() >= 110 && p.x() < 130)
  {
    prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
  }
  m_grab = true;

  const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
  if(m_value != newValue)
  {
    m_value = newValue;
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsXYZChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  m_value = ossia::vec3f{0., 0., 0.};
  m_grab = true;
  sliderMoved();
  sliderReleased();
  m_grab = false;
  update();
  event->accept();
}

void QGraphicsXYZChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto p = event->pos();
    if(p.x() < 100.)
    {
      prev_v[0] = qBound(0., p.x() / 100., 1.);
      prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    else if(p.x() >= 110 && p.x() <= 130)
    {
      prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    m_grab = true;

    const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
    if(m_value != newValue)
    {
      m_value = newValue;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsXYZChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto p = event->pos();
    if(p.x() < 100.)
    {
      prev_v[0] = qBound(0., p.x() / 100., 1.);
      prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    else if(p.x() >= 110 && p.x() < 130)
    {
      prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
    }

    const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
    if(m_value != newValue)
    {
      m_value = newValue;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsXYZChooser::boundingRect() const
{
  return QRectF{0, 0, 140, 100};
}

QGraphicsXYZSpinboxChooser::QGraphicsXYZSpinboxChooser(QGraphicsItem* parent)
    : m_sb{QGraphicsSpinbox{this}, QGraphicsSpinbox{this}, QGraphicsSpinbox{this}}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setFlag(ItemHasNoContents);

  m_sb[0].setPos(0, 2);
  m_sb[1].setPos(45, 2);
  m_sb[2].setPos(45 * 2, 2);

  connect(
      &m_sb[0], &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_sb[1], &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_sb[2], &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_sb[0], &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
  connect(
      &m_sb[1], &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
  connect(
      &m_sb[2], &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
}

QGraphicsXYZSpinboxChooser::~QGraphicsXYZSpinboxChooser() = default;

void QGraphicsXYZSpinboxChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

std::array<float, 3> QGraphicsXYZSpinboxChooser::value() const noexcept
{
  return {m_sb[0].value(), m_sb[1].value(), m_sb[2].value()};
}

std::array<float, 3> QGraphicsXYZSpinboxChooser::getMin() const noexcept
{
  return {m_sb[0].min, m_sb[1].min, m_sb[2].min};
}
std::array<float, 3> QGraphicsXYZSpinboxChooser::getMax() const noexcept
{
  return {m_sb[0].max, m_sb[1].max, m_sb[2].max};
}

ossia::vec3f
QGraphicsXYZSpinboxChooser::scaledValue(float x, float y, float z) const noexcept
{
  return {
      m_sb[0].min + x * (m_sb[0].max - m_sb[0].min),
      m_sb[1].min + y * (m_sb[1].max - m_sb[1].min),
      m_sb[2].min + z * (m_sb[2].max - m_sb[2].min)};
}

void QGraphicsXYZSpinboxChooser::setValue(ossia::vec3f v)
{
  m_sb[0].setValue(v[0]);
  m_sb[1].setValue(v[1]);
  m_sb[2].setValue(v[2]);
  update();
}

void QGraphicsXYZSpinboxChooser::setRange(ossia::vec3f min, ossia::vec3f max)
{
  m_sb[0].setRange(min[0], max[0]);
  m_sb[1].setRange(min[1], max[1]);
  m_sb[2].setRange(min[2], max[2]);
}

QRectF QGraphicsXYZSpinboxChooser::boundingRect() const
{
  return m_rect;
}
}
