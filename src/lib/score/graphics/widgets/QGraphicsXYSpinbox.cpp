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

std::array<float, 2> QGraphicsIntXYSpinboxChooser::value() const noexcept
{
  return {float(m_x.value()), float(m_y.value())};
}

std::array<float, 2> QGraphicsIntXYSpinboxChooser::getMin() const noexcept
{
  return {(float)m_x.min, (float)m_y.min};
}
std::array<float, 2> QGraphicsIntXYSpinboxChooser::getMax() const noexcept
{
  return {(float)m_x.max, (float)m_y.max};
}

ossia::vec2f QGraphicsIntXYSpinboxChooser::scaledValue(float x, float y) const noexcept
{
  return {
      (float)(m_x.min + x * (m_x.max - m_x.min)),
      (float)(m_y.min + y * (m_y.max - m_y.min))};
}

void QGraphicsIntXYSpinboxChooser::setValue(ossia::vec2f v)
{
  m_x.setValue(v[0]);
  m_y.setValue(v[1]);
  update();
}

void QGraphicsIntXYSpinboxChooser::setRange(ossia::vec2f min, ossia::vec2f max)
{
  m_x.setRange(min[0], max[0]);
  m_y.setRange(min[1], max[1]);
}

QRectF QGraphicsIntXYSpinboxChooser::boundingRect() const
{
  return m_rect;
}
}
