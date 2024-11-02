#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsSpinbox final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsSpinbox)
  SCORE_GRAPHICS_ITEM_TYPE(200)
  Q_DISABLE_COPY(QGraphicsSpinbox)

  friend struct DefaultControlImpl;
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect{0., 0., 40., 20.};

private:
  double m_value{}, m_execValue{};
  bool m_grab{};
  bool m_hasExec{};
  bool m_noValueChangeOnMove{};

public:
  float min{}, max{}, init{};
  explicit QGraphicsSpinbox(QGraphicsItem* parent);
  ~QGraphicsSpinbox();

  void setValue(double r);
  void setExecutionValue(double r);
  void resetExecution();
  void setRange(double min, double max, double init);
  void setNoValueChangeOnMove(bool);
  double value() const;

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

class SCORE_LIB_BASE_EXPORT QGraphicsIntSpinbox final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsIntSpinbox)
  SCORE_GRAPHICS_ITEM_TYPE(210)
  Q_DISABLE_COPY(QGraphicsIntSpinbox)

  friend struct DefaultControlImpl;
  friend struct DefaultGraphicsSpinboxImpl;
  QRectF m_rect{0., 0., 40., 20.};

private:
  double m_value{}, m_execValue{};
  bool m_grab{};
  bool m_hasExec{};
  bool m_noValueChangeOnMove{};

public:
  double min{}, max{}, init{};
  explicit QGraphicsIntSpinbox(QGraphicsItem* parent);
  ~QGraphicsIntSpinbox();

  void setValue(double r);
  void setExecutionValue(double r);
  void resetExecution();
  void setRange(double min, double max, double init);
  void setNoValueChangeOnMove(bool);
  int value() const;

  bool moving = false;

  double unmap(double v) const noexcept
  {
    return (double(v) - double(min)) / (double(max) - double(min));
  }
  double map(double v) const noexcept
  {
    return (double(v) * (double(max) - double(min))) + double(min);
  }

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
