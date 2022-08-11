#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsSliderBase.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsSlider
    : public QObject
    , public QGraphicsSliderBase<QGraphicsSlider>
{
  W_OBJECT(QGraphicsSlider)
  friend struct DefaultGraphicsSliderImpl;
  friend struct QGraphicsSliderBase<QGraphicsSlider>;

protected:
  double m_value{};
  double m_execValue{};

public:
  double min{}, max{};

private:
  bool m_grab{};
  bool m_hasExec{};

public:
  explicit QGraphicsSlider(QGraphicsItem* parent);

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

  void setRange(double min, double max);
  void setValue(double v);
  double value() const;
  void setExecutionValue(double v);
  void resetExecution();

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
