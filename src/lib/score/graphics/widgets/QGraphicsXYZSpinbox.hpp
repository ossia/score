#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsXYZSpinboxChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsXYZSpinboxChooser)
  SCORE_GRAPHICS_ITEM_TYPE(290)
  QRectF m_rect{0., 0., 150., 24.};
  QGraphicsSpinbox m_x, m_y, m_z;

public:
  explicit QGraphicsXYZSpinboxChooser(QGraphicsItem* parent);
  ~QGraphicsXYZSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(std::array<float, 3> v);
  void setValue(std::array<double, 3> v);
  void setRange(
      ossia::vec3f min = {0.f, 0.f, 0.f}, ossia::vec3f max = {1.f, 1.f, 1.f},
      ossia::vec3f init = {0.f, 0.f, 0.f});
  std::array<double, 3> value() const noexcept;
  std::array<double, 3> getMin() const noexcept;
  std::array<double, 3> getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  std::array<double, 3> scaledValue(double x, double y, double z) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
