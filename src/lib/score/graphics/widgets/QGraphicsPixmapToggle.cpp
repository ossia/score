#include <score/graphics/widgets/QGraphicsPixmapToggle.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapToggle);

namespace score
{
QGraphicsPixmapToggle::QGraphicsPixmapToggle(
    QPixmap pressed,
    QPixmap released,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_released{std::move(released)}
{
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsPixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
}

void QGraphicsPixmapToggle::setState(bool toggled)
{
  if (toggled != m_toggled)
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
}
