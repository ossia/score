#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsHSVChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsHSVChooser)
  Q_INTERFACES(QGraphicsItem)
private:
  QRectF m_rect{0., 0., 140., 100.};

  double h{}, s{}, v{};
  double prev_v{-1.};
  ossia::vec4f m_value{};
  QImage hs_zone;
  bool m_grab{};

public:
  QGraphicsHSVChooser(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setRgbaValue(ossia::vec4f v);
  void setHsvValue(ossia::vec4f v);
  ossia::vec4f rgbaValue() const;
  ossia::vec4f hsvValue() const;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};
}
