#include "GraphicWidgets.hpp"

#include <score/actions/ActionManager.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/model/Skin.hpp>

#include <QDebug>
#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QWidget>
#include <QWindow>
#include <QApplication>
#include <QScreen>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapButton)
W_OBJECT_IMPL(score::QGraphicsSelectablePixmapToggle)
W_OBJECT_IMPL(score::QGraphicsPixmapToggle)
W_OBJECT_IMPL(score::QGraphicsSlider)
W_OBJECT_IMPL(score::QGraphicsKnob)
W_OBJECT_IMPL(score::QGraphicsLogSlider)
W_OBJECT_IMPL(score::QGraphicsLogKnob)
W_OBJECT_IMPL(score::QGraphicsIntSlider)
W_OBJECT_IMPL(score::QGraphicsComboSlider)
W_OBJECT_IMPL(score::QGraphicsEnum)
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

QGraphicsSelectablePixmapToggle::QGraphicsSelectablePixmapToggle(
    QPixmap pressed,
    QPixmap pressed_selected,
    QPixmap released,
    QPixmap released_selected,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_pressed_selected{std::move(pressed_selected)}
    , m_released{std::move(released)}
    , m_released_selected{std::move(released_selected)}
{
  setCursor(Qt::CrossCursor);
}

void QGraphicsSelectablePixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ?
                (m_selected ? m_pressed_selected : m_pressed)
              : (m_selected ? m_released_selected : m_released));
}

void QGraphicsSelectablePixmapToggle::setSelected(bool selected)
{
  if(selected != m_selected)
  {
    m_selected = selected;
    setPixmap(m_toggled ?
                (m_selected ? m_pressed_selected : m_pressed)
              : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::setState(bool toggled)
{
  if(toggled != m_toggled)
  {
    m_toggled = toggled;
    setPixmap(m_toggled ?
                (m_selected ? m_pressed_selected : m_pressed)
              : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ?
                (m_selected ? m_pressed_selected : m_pressed)
              : (m_selected ? m_released_selected : m_released));
  toggled(m_toggled);
  event->accept();
}

void QGraphicsSelectablePixmapToggle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsSelectablePixmapToggle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
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
  if(toggled != m_toggled)
  {
    m_toggled = toggled;
    setPixmap(m_toggled ? m_pressed : m_released);
  }
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

QGraphicsSlider::QGraphicsSlider(QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_rect{0., 0., 150., 15.}
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





QGraphicsKnob::QGraphicsKnob(QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_rect{0., 0., 40., 40.}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsKnob::value() const
{
  return m_value;
}


void QGraphicsKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 2),
      painter,
      widget);
}


QGraphicsLogSlider::QGraphicsLogSlider(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_rect{0., 0., 150., 15.}
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




QGraphicsLogKnob::QGraphicsLogKnob(QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_rect{0., 0., 40., 40.}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsLogKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogKnob::value() const
{
  return m_value;
}


void QGraphicsLogKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsLogKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsLogKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsLogKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(std::exp2(min + value() * (max - min)), 'f', 3),
      painter,
      widget);
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



QGraphicsEnum::QGraphicsEnum(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsEnum::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  m_smallRect = r.adjusted(2, 2, -2, -2);
}

void QGraphicsEnum::setValue(int v)
{
  m_value = ossia::clamp(v, 0, array.size() - 1);
  update();
}

int QGraphicsEnum::value() const
{
  return m_value;
}

void QGraphicsEnum::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns - 2;
  const double h = m_smallRect.height() / rows - 2;
  int i = 0;
  for(const QString& str : array)
  {
    QRectF rect{col * w, row * h, w, h};
    if(rect.contains(event->pos()))
    {
      m_clicking = i;
      update();
      return;
    }

    col++;
    if(col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }

  m_clicking = -1;
  update();
}

void QGraphicsEnum::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsEnum::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(m_clicking != -1)
  {
    m_value = m_clicking;
    m_clicking = -1;
    currentIndexChanged(m_value);
  }
  update();
}

QRectF QGraphicsEnum::boundingRect() const
{
  return m_rect;
}

void QGraphicsEnum::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& style = score::Skin::instance();

  static QPen border(style.Base1.color(), 1);
  static QPen borderClicking(style.Smooth3.color(), 1);
  static QPen text{style.Base1.color()};
  static QFont textFont{style.MonoFontSmall};
  static QPen currentText(style.Smooth2.color());
  static QBrush bg(style.SliderBrush);

  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns - 2;
  const double h = m_smallRect.height() / rows - 2;

  painter->setBrush(bg);
  int i = 0;
  for(const QString& str : array)
  {
    QRectF rect{col * w, row * h, w, h};
    painter->setPen(i != m_clicking ? border : borderClicking);
    painter->drawRect(rect);

    painter->setPen(i != m_value ? text : currentText);
    painter->setFont(textFont);
    painter->drawText(rect, str, QTextOption(Qt::AlignCenter));
    col++;
    if(col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }
}

}
