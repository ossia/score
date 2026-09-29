#pragma once
#include <functional>
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
// dotted and triplet, from 1/64th to four bars. A right-click on a free time
// types it in, in seconds.
//
// The value is vec2f{x, sync}: x is a normalized 0..1 position when free
// (the consumer maps it to seconds), a fraction of a whole note when synced.
class SCORE_LIB_BASE_EXPORT QGraphicsTimeChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsTimeChooser)
  SCORE_GRAPHICS_ITEM_TYPE(230)
  friend struct DefaultControlImpl;
  friend struct DefaultGraphicsKnobImpl;

public:
  double min{}, max{1.}, init{};
  bool moving{};

  //! Free mode: seconds for a knob position, and the reverse. Linear in the
  //! range unless the control's normalizer says otherwise (the factory sets
  //! them from it, so that the readout and the value agree).
  std::function<double(double)> positionToSeconds;
  std::function<double(double)> secondsToPosition;

  explicit QGraphicsTimeChooser(QGraphicsItem* parent);
  ~QGraphicsTimeChooser();

  void setRange(double min, double max, double init);
  void setRect(const QRectF& r);
  void setValue(ossia::vec2f v);

  [[nodiscard]] ossia::vec2f value() const noexcept;
  void setExecutionValue(ossia::vec2f v);
  [[nodiscard]] double executionPosition() const noexcept { return m_execValue; }
  void resetExecution();

  void syncChanged(bool sync);

  //! What a click on the readout does: free, then synced to straight, dotted
  //! and triplet values of the same note, then free again.
  void cycleMode();

  //! Synced values the knob steps through (Alt or Shift: all of them).
  enum class Feel : uint8_t
  {
    Straight,
    Dotted,
    Triplet
  };
  [[nodiscard]] bool synced() const noexcept { return m_sync; }
  //! The readout in free mode: seconds, milliseconds, one decimal under 10 ms.
  [[nodiscard]] QString freeText() const;
  [[nodiscard]] Feel feel() const noexcept { return m_feel; }

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
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  //! Free mode: the seconds of a knob position, and the position of a time
  //! (what the right-click type-in box shows and sets).
  double map(double position) const noexcept;
  double unmap(double seconds) const noexcept;
  bool onReadout(QPointF pos) const noexcept;

  int syncIndex() const noexcept;
  //! Knob position, 0..1, for a {x, sync} pair in the current mode
  double position(ossia::vec2f v) const noexcept;

  QRectF m_rect{0., 0., 35., 35.};

  double m_value{};     // knob position, 0..1, in the current mode
  double m_execValue{};
  double m_other01{};   // remembered position of the inactive mode
  bool m_sync{};
  Feel m_feel{Feel::Straight};
  bool m_grab{};
  bool m_hasExec{};
  bool m_hover{};
};
}
