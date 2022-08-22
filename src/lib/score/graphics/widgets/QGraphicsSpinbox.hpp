#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score_lib_base_export.h>

#include <QGraphicsItem>
#include <QObject>
#include <verdigris>
namespace score
{
struct DefaultGraphicsKnobImpl;
class SCORE_LIB_BASE_EXPORT QGraphicsSpinbox final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsSpinbox)
  Q_INTERFACES(QGraphicsItem)
  Q_DISABLE_COPY(QGraphicsSpinbox)

  friend struct DefaultGraphicsKnobImpl;
  QRectF m_rect{0., 0., 40., 20.};

private:
  float m_value{}, m_execValue{};
  bool m_grab{};
  bool m_hasExec{};

public:
  float min{}, max{};
  explicit QGraphicsSpinbox(QGraphicsItem* parent);
  ~QGraphicsSpinbox();

  void setValue(float r);
  void setExecutionValue(float r);
  void resetExecution();
  void setRange(float min, float max);
  float value() const;

  bool moving = false;

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
