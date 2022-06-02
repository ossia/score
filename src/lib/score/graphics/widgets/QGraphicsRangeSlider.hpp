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
    bool moving = false;

    explicit QGraphicsRangeSlider(QGraphicsItem* parent);

    void setStart(double start);
    void setEnd(double end);
    void setRange(double min, double max);

    void setValue(ossia::vec2f value);
    ossia::vec2f value() const noexcept;
    void setExecutionValue(ossia::vec2f) { } // TODO
    void resetExecution() { } // TODO

    void startChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, startChanged, arg_1)
    void endChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, endChanged, arg_1)

    void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
    void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)
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
