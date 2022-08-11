#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsXYChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsXYChooser)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{0., 0., 100., 100.};

private:
  ossia::vec2f m_value{}, m_min{}, m_max{};
  bool m_grab{};

public:
  QGraphicsXYChooser(QGraphicsItem* parent);

  void setPoint(const QPointF& r);
  void setValue(ossia::vec2f v);
  void setRange(ossia::vec2f min, ossia::vec2f max);
  ossia::vec2f value() const;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  ossia::vec2f scaledValue(float x, float y) const noexcept;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
