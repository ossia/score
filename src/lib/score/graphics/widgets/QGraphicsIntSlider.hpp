#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsSliderBase.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider final
    : public QObject
    , public QGraphicsSliderBase<QGraphicsIntSlider>
{
  W_OBJECT(QGraphicsIntSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;
  friend struct QGraphicsSliderBase<QGraphicsIntSlider>;
  int m_value{}, m_execValue{}, m_min{}, m_max{};
  bool m_grab{};
  bool m_hasExec{};

public:
  QGraphicsIntSlider(QGraphicsItem* parent);

  void setValue(int v);
  void setExecutionValue(int v);
  void resetExecution();
  void setRange(int min, int max);
  int value() const;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  double getHandleX() const;
  double getExecHandleX() const;
};
}
