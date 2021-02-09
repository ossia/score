#include <score/graphics/widgets/QGraphicsPixmapButton.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapButton)

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
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
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

}
