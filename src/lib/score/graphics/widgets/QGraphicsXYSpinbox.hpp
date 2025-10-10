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
struct DefaultGraphicsSpinboxImpl;
class SCORE_LIB_BASE_EXPORT QGraphicsXYSpinboxChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsXYSpinboxChooser)
  SCORE_GRAPHICS_ITEM_TYPE(260)
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect;
  QGraphicsSpinbox m_x, m_y;

  bool m_isRange{};

public:
  explicit QGraphicsXYSpinboxChooser(bool isRange, QGraphicsItem* parent);
  ~QGraphicsXYSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(std::array<float, 2> v);
  void setValue(std::array<double, 2> v);
  void setRange(
      ossia::vec2f min = {0.f, 0.f}, ossia::vec2f max = {1.f, 1.f},
      ossia::vec2f init = {0.f, 0.f});

  std::array<double, 2> value() const noexcept;
  std::array<double, 2> getMin() const noexcept;
  std::array<double, 2> getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  std::array<double, 2> scaledValue(double x, double y) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsIntXYSpinboxChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsIntXYSpinboxChooser)
  SCORE_GRAPHICS_ITEM_TYPE(270)
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect;
  QGraphicsIntSpinbox m_x, m_y;

  bool m_grab{};
  bool m_isRange{};

public:
  explicit QGraphicsIntXYSpinboxChooser(bool isRange, QGraphicsItem* parent);
  ~QGraphicsIntXYSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(std::array<float, 2> v);
  void setValue(std::array<double, 2> v);
  void setRange(
      ossia::vec2f min = {0.f, 0.f}, ossia::vec2f max = {1.f, 1.f},
      ossia::vec2f init = {0.f, 0.f});

  std::array<double, 2> value() const noexcept;
  std::array<double, 2> getMin() const noexcept;
  std::array<double, 2> getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  std::array<double, 2> scaledValue(double x, double y) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
