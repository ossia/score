#pragma once
#include <score/graphics/widgets/QGraphicsToggleGrid.hpp>

namespace score
{
/**
 * The 16 MIDI channels as two rows of toggles, every channel on at first. Its
 * text form is made of channel numbers and ranges: "1-4, 10".
 *
 * The value is an ossia list of 16 booleans, channel 1 first.
 */
class SCORE_LIB_BASE_EXPORT QGraphicsChannelMask final : public QGraphicsToggleGrid
{
  W_OBJECT(QGraphicsChannelMask)
  SCORE_GRAPHICS_ITEM_TYPE(320)

public:
  explicit QGraphicsChannelMask(QGraphicsItem* parent);
  ~QGraphicsChannelMask();

  QString toText() const override;
  bool setText(const QString& text) override;

private:
  QString editTitle() const override;
  QString editHint() const override;
  QString editError() const override;
};
}
