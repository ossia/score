#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <functional>
#include <verdigris>

class QMimeData;
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsSelectablePixmapToggle final
    : public QObject
    , public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsSelectablePixmapToggle)
  SCORE_GRAPHICS_ITEM_TYPE(170)

  const QPixmap m_pressed, m_pressed_selected, m_released, m_released_selected;
  bool m_toggled{};
  bool m_selected{};

public:
  QGraphicsSelectablePixmapToggle(
      QPixmap pressed, QPixmap pressed_selected, QPixmap released,
      QPixmap released_selected, QGraphicsItem* parent);

  void toggle();
  void setSelected(bool selected);
  void setState(bool toggled);

  bool state() const noexcept { return m_toggled; }

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsDraggablePixmap final
    : public QObject
    , public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsDraggablePixmap)
  SCORE_GRAPHICS_ITEM_TYPE(180)

  const QPixmap m_pressed, m_pressed_selected, m_released, m_released_selected;

public:
  QGraphicsDraggablePixmap(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  std::function<void(QMimeData&)> createDrag;
  std::function<void(Qt::MouseButton, QPointF)> click;

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  bool m_didDrag{};
};
}
