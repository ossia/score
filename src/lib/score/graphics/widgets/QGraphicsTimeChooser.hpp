#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsToggle.hpp>

#include <ossia/network/value/vec.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsTimeChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsTimeChooser)
  Q_INTERFACES(QGraphicsItem)

public:
  QGraphicsKnob knob;
  QGraphicsCombo combo;
  QGraphicsToggle toggle;

  explicit QGraphicsTimeChooser(QGraphicsItem* parent);
  ~QGraphicsTimeChooser();

  void syncChanged(bool sync);

  void setRect(const QRectF& r);
  void setValue(ossia::vec2f v);

  [[nodiscard]] ossia::vec2f value() const noexcept;
  void setExecutionValue(ossia::vec2f v);
  void resetExecution();

  QRectF boundingRect() const override;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)
private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
