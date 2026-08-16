#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia/network/value/vec.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
// A time value that is either free-running (seconds, continuous) or
// tempo-synced (a musical division). Renders as a standard knob with the
// readout below it; dragging the knob changes the value, clicking the
// readout toggles the mode (with per-mode memory so no value jumps). In
// sync mode the knob steps through the whole division table: straight,
// dotted and triplet, from 1/64th to four bars.
//
// The value is vec2f{x, sync}: x is a normalized 0..1 position when free
// (the consumer maps it to seconds), a fraction of a whole note when synced.
class SCORE_LIB_BASE_EXPORT QGraphicsTimeChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsTimeChooser)
  SCORE_GRAPHICS_ITEM_TYPE(230)
  friend struct DefaultGraphicsKnobImpl;

public:
  double min{}, max{1.}, init{};
  bool moving{};

  explicit QGraphicsTimeChooser(QGraphicsItem* parent);
  ~QGraphicsTimeChooser();

  void setRange(double min, double max, double init);
  void setRect(const QRectF& r);
  void setValue(ossia::vec2f v);

  [[nodiscard]] ossia::vec2f value() const noexcept;
  void setExecutionValue(ossia::vec2f v);
  void resetExecution();

  void syncChanged(bool sync);

  QRectF boundingRect() const override;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  int syncIndex() const noexcept;
  QString freeText() const;

  QRectF m_rect{0., 0., 35., 35.};

  double m_value{};     // knob position, 0..1, in the current mode
  double m_execValue{};
  double m_other01{};   // remembered position of the inactive mode
  bool m_sync{};
  bool m_grab{};
  bool m_hasExec{};
  bool m_hover{};
};
}
