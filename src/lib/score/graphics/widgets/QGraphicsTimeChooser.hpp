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
// tempo-synced (a musical division). One slider-sized item, two rows:
// dragging the bar changes the value, clicking the readout row toggles the
// mode (with per-mode memory so no value jumps). In sync mode a plain drag
// steps through the straight divisions only; dragging with Alt or Shift
// steps through the full list including dotted and triplet.
//
// The value is vec2f{x, sync}: x is a normalized 0..1 position when free
// (the consumer maps it to seconds), a fraction of a whole note when synced.
class SCORE_LIB_BASE_EXPORT QGraphicsTimeChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsTimeChooser)
  SCORE_GRAPHICS_ITEM_TYPE(230)

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

  void dragTo(double posX, bool fullTable);
  QString freeText() const;

  QRectF m_rect{0., 0., 60., 23.};

  double m_value01{};    // free mode position
  int m_syncIndex{};     // index in the division table
  bool m_sync{};
  bool m_grab{};

  // Per-mode memory: toggling restores the last value of the other mode
  double m_lastFree01{};
  int m_lastSyncIndex{};

  float m_execX{};
  bool m_execSync{};
  bool m_hasExec{};
};
}
