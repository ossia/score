#include "QGraphicsXYZSpinbox.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsXYZSpinboxChooser);

namespace score
{

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
