#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

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

public:
    QGraphicsRangeSlider(QGraphicsItem* parent);

    void start();
    void end();
    void setStart(double start);
    void setEnd(double end);

public:
    void start_changed(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, start_changed, arg_1)
    void end_changed(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, end_changed, arg_1)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};
}
