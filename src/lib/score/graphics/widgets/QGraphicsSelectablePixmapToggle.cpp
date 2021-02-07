#include <score/graphics/widgets/QGraphicsSelectablePixmapToggle.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsSelectablePixmapToggle);

namespace score
{
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
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsSelectablePixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(
        m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                  : (m_selected ? m_released_selected : m_released));
}

void QGraphicsSelectablePixmapToggle::setSelected(bool selected)
{
  if (selected != m_selected)
  {
    m_selected = selected;
    setPixmap(
          m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                    : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::setState(bool toggled)
{
  if (toggled != m_toggled)
  {
    m_toggled = toggled;
    setPixmap(
          m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                    : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  setPixmap(
        m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
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
}
