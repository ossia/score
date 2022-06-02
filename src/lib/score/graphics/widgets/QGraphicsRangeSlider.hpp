#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

///TODO. list in src/plugins/score-plugin-avnd/Crousti/Concepts.hpp#L139

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsRangeSlider final
        : public QObject
        , public QGraphicsItem
{
    W_OBJECT(QGraphicsRangeSlider)
    Q_INTERFACES(QGraphicsItem)
    QRectF m_rect{defaultRangeSliderSize};

    double m_start{0.}, m_end{1.};
    double m_min{0}, m_max{1};

public:
    QGraphicsRangeSlider(QGraphicsItem* parent);

    void setStart(double start);
    void setEnd(double end);

    void setRange(double min, double max);

public:
    void start_changed(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, start_changed, arg_1)
    void end_changed(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, end_changed, arg_1)

    double unmap(double v) const noexcept { return (v - m_min) / (m_max - m_min); }
    double map(double v) const noexcept { return (v * (m_max - m_min)) + m_min; }

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  qreal y_factor{0.005};
  qreal d2s,d2c,d2e, ypos, ydiff;
  double val1, val2;

  enum Handle {
      START,
      CENTER,
      END,
      NONE
  };
  Handle handle;

  QRectF rangeRect{
      boundingRect().width()*m_start,
      boundingRect().top(),
      boundingRect().width()*(m_end-m_start),
      boundingRect().height()
  };

};
}
