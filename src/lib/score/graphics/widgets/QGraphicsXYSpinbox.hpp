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
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect{0., 0., 150., 24.};
  QGraphicsSpinbox m_x, m_y;

  bool m_grab{};
  bool m_isRange{};

public:
  explicit QGraphicsXYSpinboxChooser(bool isRange, QGraphicsItem* parent);
  ~QGraphicsXYSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(ossia::vec2f v);
  void setRange(ossia::vec2f min = {0.f, 0.f}, ossia::vec2f max = {1.f, 1.f});

  ossia::vec2f value() const noexcept;
  ossia::vec2f getMin() const noexcept;
  ossia::vec2f getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  ossia::vec2f scaledValue(float x, float y) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsIntXYSpinboxChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsIntXYSpinboxChooser)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect{0., 0., 150., 24.};
  QGraphicsIntSpinbox m_x, m_y;

  bool m_grab{};
  bool m_isRange{};

public:
  explicit QGraphicsIntXYSpinboxChooser(bool isRange, QGraphicsItem* parent);
  ~QGraphicsIntXYSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(ossia::vec2f v);
  void setRange(ossia::vec2f min = {0.f, 0.f}, ossia::vec2f max = {1.f, 1.f});

  ossia::vec2f value() const noexcept;
  ossia::vec2f getMin() const noexcept;
  ossia::vec2f getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  ossia::vec2f scaledValue(float x, float y) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
