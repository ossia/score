#include "GraphicWidgets.hpp"
#include <QDebug>
#include <score/model/Skin.hpp>
namespace score
{

QGraphicsPixmapButton::QGraphicsPixmapButton(QPixmap pressed, QPixmap released, QGraphicsItem* parent)
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



QGraphicsPixmapToggle::QGraphicsPixmapToggle(QPixmap pressed, QPixmap released, QGraphicsItem* parent)
  : QGraphicsPixmapItem{released, parent}
  , m_pressed{std::move(pressed)}
  , m_released{std::move(released)}
{

}

void QGraphicsPixmapToggle::toggle()
{
  m_toggled = !m_toggled;
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



QGraphicsSlider::QGraphicsSlider(QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsSlider::setRect(QRectF r)
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
  if(isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  if(curPos != m_value)
  {
    m_value = curPos;
    valueChanged(m_value);
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    if(curPos != m_value)
    {
      m_value = curPos;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    if(curPos != m_value)
    {
      m_value = curPos;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  painter->drawText(srect.adjusted(6, -2, -6, -1),
                    QString::number(min + value() * (max - min), 'f', 3),
                    getHandleX() > srect.width() / 2 ? QTextOption() : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
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







QGraphicsLogSlider::QGraphicsLogSlider(QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsLogSlider::setRect(QRectF r)
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
  if(isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  if(curPos != m_value)
  {
    m_value = curPos;
    valueChanged(m_value);
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsLogSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    if(curPos != m_value)
    {
      m_value = curPos;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsLogSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    if(curPos != m_value)
    {
      m_value = curPos;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsLogSlider::boundingRect() const
{
  return m_rect;
}

void QGraphicsLogSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  painter->drawText(srect.adjusted(6, -2, -6, -1),
                    QString::number(std::exp2(min + value() * (max - min)), 'f', 3),
                    getHandleX() > srect.width() / 2 ? QTextOption() : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
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
  return{getHandleX() - 4., 1., 8., m_rect.height() - 1};
}







QGraphicsIntSlider::QGraphicsIntSlider(QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsIntSlider::setRect(QRectF r)
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
  if(isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(m_min + curPos * (m_max - m_min));
  if(res != m_value)
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
  if(m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if(res != m_value)
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
  if(m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if(res != m_value)
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

void QGraphicsIntSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  painter->drawText(srect.adjusted(6, -2, -6, -1),
                    QString::number(value()),
                    getHandleX() > srect.width() / 2 ? QTextOption() : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
}

bool QGraphicsIntSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double QGraphicsIntSlider::getHandleX() const
{
  return 4 + sliderRect().width() * ((double(m_value) - m_min) / (m_max - m_min));
}

QRectF QGraphicsIntSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF QGraphicsIntSlider::handleRect() const
{
  return{getHandleX() - 4., 1., 8., m_rect.height() - 1};
}








QGraphicsComboSlider::QGraphicsComboSlider(QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsComboSlider::setRect(QRectF r)
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
  if(isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(curPos * (array.size() - 1));
  if(res != m_value)
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
  if(m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(curPos * (array.size() - 1));
    if(res != m_value)
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
  if(m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(curPos * (array.size() - 1));
    if(res != m_value)
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

void QGraphicsComboSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  painter->drawText(srect.adjusted(6, -2, -6, -1),
                    array[value()],
                    getHandleX() > srect.width() / 2 ? QTextOption() : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
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
  return{getHandleX() - 4., 1., 8., m_rect.height() - 1};
}
}
