#include "GraphicWidgets.hpp"

#include <score/actions/ActionManager.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/model/Skin.hpp>

#include <QDebug>
#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QWidget>
#include <QWindow>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapButton)
W_OBJECT_IMPL(score::QGraphicsPixmapToggle)
W_OBJECT_IMPL(score::QGraphicsSlider)
W_OBJECT_IMPL(score::QGraphicsLogSlider)
W_OBJECT_IMPL(score::QGraphicsIntSlider)
W_OBJECT_IMPL(score::QGraphicsComboSlider)
W_OBJECT_IMPL(score::DoubleSpinboxWithEnter)
namespace score
{

QGraphicsPixmapButton::QGraphicsPixmapButton(
    QPixmap pressed,
    QPixmap released,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_released{std::move(released)}
{
}

void QGraphicsPixmapButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  setPixmap(m_pressed);
  event->accept();
}

void QGraphicsPixmapButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsPixmapButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  setPixmap(m_released);
  clicked();
  event->accept();
}

QGraphicsPixmapToggle::QGraphicsPixmapToggle(
    QPixmap pressed,
    QPixmap released,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_released{std::move(released)}
{
  setCursor(Qt::CrossCursor);
}

void QGraphicsPixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
}

void QGraphicsPixmapToggle::setState(bool toggled)
{
  m_toggled = toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
}

void QGraphicsPixmapToggle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
  toggled(m_toggled);
  event->accept();
}

void QGraphicsPixmapToggle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsPixmapToggle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

QGraphicsSlider::QGraphicsSlider(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsSlider::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsSlider::value() const
{
  return m_value;
}

void QGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 3),
      painter,
      widget);
}

bool QGraphicsSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double QGraphicsSlider::getHandleX() const
{
  return 4 + sliderRect().width() * m_value;
}

QRectF QGraphicsSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF QGraphicsSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}

QGraphicsLogSlider::QGraphicsLogSlider(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogSlider::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsLogSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogSlider::value() const
{
  return m_value;
}

void QGraphicsLogSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsLogSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsLogSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsLogSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(std::exp2(min + value() * (max - min)), 'f', 3),
      painter,
      widget);
}

bool QGraphicsLogSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double QGraphicsLogSlider::getHandleX() const
{
  return 4 + sliderRect().width() * m_value;
}

QRectF QGraphicsLogSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF QGraphicsLogSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}

QGraphicsIntSlider::QGraphicsIntSlider(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsIntSlider::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsIntSlider::setValue(int v)
{
  m_value = ossia::clamp(v, m_min, m_max);
  update();
}

void QGraphicsIntSlider::setRange(int min, int max)
{
  m_min = min;
  m_max = max;
  update();
}

int QGraphicsIntSlider::value() const
{
  return m_value;
}

void QGraphicsIntSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos
      = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(m_min + curPos * (m_max - m_min));
  if (res != m_value)
  {
    m_value = res;
    valueChanged(m_value);
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsIntSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto srect = sliderRect();
    double curPos
        = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsIntSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    double curPos
        = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsIntSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsIntSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(value()),
      painter,
      widget);
}

bool QGraphicsIntSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double QGraphicsIntSlider::getHandleX() const
{
  return 4
         + sliderRect().width()
               * ((double(m_value) - m_min) / (m_max - m_min));
}

QRectF QGraphicsIntSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF QGraphicsIntSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}

QGraphicsComboSlider::QGraphicsComboSlider(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsComboSlider::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsComboSlider::setValue(int v)
{
  m_value = ossia::clamp(v, 0, array.size() - 1);
  update();
}

int QGraphicsComboSlider::value() const
{
  return m_value;
}

void QGraphicsComboSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos
      = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(curPos * (array.size() - 1));
  if (res != m_value)
  {
    m_value = res;
    valueChanged(m_value);
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsComboSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto srect = sliderRect();
    double curPos
        = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(curPos * (array.size() - 1));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsComboSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    double curPos
        = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(curPos * (array.size() - 1));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsComboSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsComboSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), array[value()], painter, widget);
}

bool QGraphicsComboSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double QGraphicsComboSlider::getHandleX() const
{
  return 4 + sliderRect().width() * (double(m_value) / (array.size() - 1));
}

QRectF QGraphicsComboSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF QGraphicsComboSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}
}
