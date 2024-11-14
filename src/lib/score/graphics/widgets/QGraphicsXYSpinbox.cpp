#include "QGraphicsXYSpinbox.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsXYSpinboxChooser);
W_OBJECT_IMPL(score::QGraphicsIntXYSpinboxChooser);

namespace score
{

QGraphicsXYSpinboxChooser::QGraphicsXYSpinboxChooser(bool isRange, QGraphicsItem* parent)
    : m_x{this}
    , m_y{this}
    , m_isRange{isRange}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setFlag(ItemHasNoContents);

  m_x.setPos(0, 2);
  m_y.setPos(45, 2);

  if(m_isRange)
  {
    connect(&m_x, &QGraphicsSpinbox::sliderMoved, this, [this]() {
      if(m_x.value() >= m_y.value())
        m_y.setValue(m_x.value() + 0.0000001);
      sliderMoved();
    });
    connect(&m_y, &QGraphicsSpinbox::sliderMoved, this, [this]() {
      if(m_y.value() <= m_x.value())
        m_x.setValue(m_y.value() - 0.0000001);
      sliderMoved();
    });
  }
  else
  {
    connect(
        &m_x, &QGraphicsSpinbox::sliderMoved, this,
        &QGraphicsXYSpinboxChooser::sliderMoved);
    connect(
        &m_y, &QGraphicsSpinbox::sliderMoved, this,
        &QGraphicsXYSpinboxChooser::sliderMoved);
  }
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

std::array<double, 2> QGraphicsXYSpinboxChooser::value() const noexcept
{
  return {m_x.value(), m_y.value()};
}

std::array<double, 2> QGraphicsXYSpinboxChooser::getMin() const noexcept
{
  return {m_x.min, m_y.min};
}
std::array<double, 2> QGraphicsXYSpinboxChooser::getMax() const noexcept
{
  return {m_x.max, m_y.max};
}
std::array<double, 2>
QGraphicsXYSpinboxChooser::scaledValue(double x, double y) const noexcept
{
  return {(m_x.min + x * (m_x.max - m_x.min)), (m_y.min + y * (m_y.max - m_y.min))};
}

void QGraphicsXYSpinboxChooser::setValue(ossia::vec2f v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  update();
}

void QGraphicsXYSpinboxChooser::setRange(
    ossia::vec2f min, ossia::vec2f max, ossia::vec2f init)
{
  m_x.setRange(min[0], max[0], init[0]);
  m_y.setRange(min[1], max[1], init[1]);
}

QRectF QGraphicsXYSpinboxChooser::boundingRect() const
{
  return m_rect;
}

QGraphicsIntXYSpinboxChooser::QGraphicsIntXYSpinboxChooser(
    bool isRange, QGraphicsItem* parent)
    : m_x{this}
    , m_y{this}
    , m_isRange{isRange}

{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setFlag(ItemHasNoContents);

  m_x.setPos(0, 2);
  m_y.setPos(45, 2);

  if(m_isRange)
  {
    connect(&m_x, &QGraphicsIntSpinbox::sliderMoved, this, [this]() {
      if(m_x.value() >= m_y.value())
        m_y.setValue(m_x.value() + 0.0000001);
      sliderMoved();
    });
    connect(&m_y, &QGraphicsIntSpinbox::sliderMoved, this, [this]() {
      if(m_y.value() <= m_x.value())
        m_x.setValue(m_y.value() - 0.0000001);
      sliderMoved();
    });
  }
  else
  {
    connect(
        &m_x, &QGraphicsIntSpinbox::sliderMoved, this,
        &QGraphicsIntXYSpinboxChooser::sliderMoved);
    connect(
        &m_y, &QGraphicsIntSpinbox::sliderMoved, this,
        &QGraphicsIntXYSpinboxChooser::sliderMoved);
  }
  connect(
      &m_x, &QGraphicsIntSpinbox::sliderReleased, this,
      &QGraphicsIntXYSpinboxChooser::sliderReleased);
  connect(
      &m_y, &QGraphicsIntSpinbox::sliderReleased, this,
      &QGraphicsIntXYSpinboxChooser::sliderReleased);
}

QGraphicsIntXYSpinboxChooser::~QGraphicsIntXYSpinboxChooser() = default;

void QGraphicsIntXYSpinboxChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

std::array<double, 2> QGraphicsIntXYSpinboxChooser::value() const noexcept
{
  return {(double)m_x.value(), (double)m_y.value()};
}

std::array<double, 2> QGraphicsIntXYSpinboxChooser::getMin() const noexcept
{
  return {(double)m_x.min, (double)m_y.min};
}

std::array<double, 2> QGraphicsIntXYSpinboxChooser::getMax() const noexcept
{
  return {(double)m_x.max, (double)m_y.max};
}

std::array<double, 2>
QGraphicsIntXYSpinboxChooser::scaledValue(double x, double y) const noexcept
{
  return {(m_x.min + x * (m_x.max - m_x.min)), (m_y.min + y * (m_y.max - m_y.min))};
}

void QGraphicsIntXYSpinboxChooser::setValue(ossia::vec2f v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  update();
}
void QGraphicsIntXYSpinboxChooser::setValue(std::array<double, 2> v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  update();
}

void QGraphicsIntXYSpinboxChooser::setRange(
    ossia::vec2f min, ossia::vec2f max, ossia::vec2f init)
{
  m_x.setRange(min[0], max[0], init[0]);
  m_y.setRange(min[1], max[1], init[1]);
}

QRectF QGraphicsIntXYSpinboxChooser::boundingRect() const
{
  return m_rect;
}
}
