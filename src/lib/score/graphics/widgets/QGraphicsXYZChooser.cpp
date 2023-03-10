#include <score/graphics/widgets/QGraphicsXYZChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsXYZChooser);
W_OBJECT_IMPL(score::QGraphicsXYSpinboxChooser);
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

QGraphicsXYSpinboxChooser::QGraphicsXYSpinboxChooser(QGraphicsItem* parent)
    : m_x{this}
    , m_y{this}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setFlag(ItemHasNoContents);

  m_x.setPos(0, 2);
  m_y.setPos(45, 2);

  connect(
      &m_x, &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYSpinboxChooser::sliderMoved);
  connect(
      &m_y, &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYSpinboxChooser::sliderMoved);
  connect(
      &m_x, &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYSpinboxChooser::sliderReleased);
  connect(
      &m_y, &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYSpinboxChooser::sliderReleased);
}

QGraphicsXYSpinboxChooser::~QGraphicsXYSpinboxChooser() = default;

void QGraphicsXYSpinboxChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

std::array<float, 2> QGraphicsXYSpinboxChooser::value() const noexcept
{
  return {m_x.value(), m_y.value()};
}

std::array<float, 2> QGraphicsXYSpinboxChooser::getMin() const noexcept
{
  return {m_x.min, m_y.min};
}
std::array<float, 2> QGraphicsXYSpinboxChooser::getMax() const noexcept
{
  return {m_x.max, m_y.max};
}

ossia::vec2f QGraphicsXYSpinboxChooser::scaledValue(float x, float y) const noexcept
{
  return {m_x.min + x * (m_x.max - m_x.min), m_y.min + y * (m_y.max - m_y.min)};
}

void QGraphicsXYSpinboxChooser::setValue(ossia::vec2f v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  update();
}

void QGraphicsXYSpinboxChooser::setRange(ossia::vec2f min, ossia::vec2f max)
{
  m_x.setRange(min[0], max[0]);
  m_y.setRange(min[1], max[1]);
}

QRectF QGraphicsXYSpinboxChooser::boundingRect() const
{
  return m_rect;
}

QGraphicsXYZSpinboxChooser::QGraphicsXYZSpinboxChooser(QGraphicsItem* parent)
    : m_x{this}
    , m_y{this}
    , m_z{this}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setFlag(ItemHasNoContents);

  m_x.setPos(0, 2);
  m_y.setPos(45, 2);
  m_z.setPos(45 * 2, 2);

  connect(
      &m_x, &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_y, &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_z, &QGraphicsSpinbox::sliderMoved, this,
      &QGraphicsXYZSpinboxChooser::sliderMoved);
  connect(
      &m_x, &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
  connect(
      &m_y, &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
  connect(
      &m_z, &QGraphicsSpinbox::sliderReleased, this,
      &QGraphicsXYZSpinboxChooser::sliderReleased);
}

QGraphicsXYZSpinboxChooser::~QGraphicsXYZSpinboxChooser() = default;

void QGraphicsXYZSpinboxChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

std::array<float, 3> QGraphicsXYZSpinboxChooser::value() const noexcept
{
  return {m_x.value(), m_y.value(), m_z.value()};
}

std::array<float, 3> QGraphicsXYZSpinboxChooser::getMin() const noexcept
{
  return {m_x.min, m_y.min, m_z.min};
}
std::array<float, 3> QGraphicsXYZSpinboxChooser::getMax() const noexcept
{
  return {m_x.max, m_y.max, m_z.max};
}

ossia::vec3f
QGraphicsXYZSpinboxChooser::scaledValue(float x, float y, float z) const noexcept
{
  return {
      m_x.min + x * (m_x.max - m_x.min), m_y.min + y * (m_y.max - m_y.min),
      m_z.min + z * (m_z.max - m_z.min)};
}

void QGraphicsXYZSpinboxChooser::setValue(ossia::vec3f v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  m_z.setValue(v[2]);
  update();
}

void QGraphicsXYZSpinboxChooser::setRange(ossia::vec3f min, ossia::vec3f max)
{
  m_x.setRange(min[0], max[0]);
  m_y.setRange(min[1], max[1]);
  m_z.setRange(min[2], max[2]);
}

QRectF QGraphicsXYZSpinboxChooser::boundingRect() const
{
  return m_rect;
}
}
