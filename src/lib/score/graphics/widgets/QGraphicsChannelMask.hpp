#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia/network/value/value.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <array>

#include <verdigris>

namespace score
{
/**
 * The 16 MIDI channels as two rows of toggles. A click toggles a channel and a
 * drag paints the same state over the others; the context menu sets all, none,
 * inverts, and edits the set as text ("1-4, 10").
 *
 * The value is an ossia list of 16 booleans, channel 1 first.
 */
class SCORE_LIB_BASE_EXPORT QGraphicsChannelMask final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsChannelMask)
  SCORE_GRAPHICS_ITEM_TYPE(320)

public:
  explicit QGraphicsChannelMask(QGraphicsItem* parent);
  ~QGraphicsChannelMask();

  void setValue(const ossia::value& v);
  ossia::value value() const;

  QString toText() const;
  bool setText(const QString& text);

  bool moving = false;

  QRectF boundingRect() const override;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  QRectF cellRect(int channel) const noexcept;
  int channelAt(QPointF p) const noexcept;
  void set(std::array<bool, 16> on);

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  std::array<bool, 16> m_on{};
  //! The state a drag paints, set by the press
  bool m_paint{};
  bool m_grab{};
};
}
