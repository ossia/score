#pragma once
#include <score/model/ColorReference.hpp>
#include <score/model/Skin.hpp>

#include <QBrush>
#include <QColor>
#include <QPen>
#include <qnamespace.h>

#include <score_lib_process_export.h>
namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT Style
{
  Style(score::Skin&) noexcept;

  Style(const Style&) = delete;
  Style(Style&&) = delete;
  Style& operator=(const Style&) = delete;
  Style& operator=(Style&&) = delete;

  void setIntervalWidth(double w);

  static Style& instance() noexcept;
  score::Skin& skin;

  static const QBrush& IntervalBase(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalSelected(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalDropTarget(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalPlayFill(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalPlayDashFill(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalWaitingDashFill(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalLoop(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalWarning(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalInvalid(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalMuted(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalDefaultLabel(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalDefaultBackground(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& RackSideBorder(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& IntervalFullViewParentSelected(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& IntervalHeaderText(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalHeaderBottomLine(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalHeaderRackHidden(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& IntervalHeaderSideBorder(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& ProcessViewBorder(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& SlotFocus(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& SlotOverlayBorder(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& SlotOverlay(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& SlotHandle(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& TimenodeDefault(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& TimenodeSelected(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& EventDefault(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& EventWaiting(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& EventPending(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& EventHappened(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& EventDisposed(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& EventSelected(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& ConditionDefault(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& ConditionWaiting(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& ConditionDisabled(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& ConditionFalse(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& ConditionTrue(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& StateOutline(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& StateSelected(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& StateDot(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& Background(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& ProcessPanelBackground(const score::Brush& b)
  { return b.main.brush; }
  const QBrush& ProcessPanelBackground()
  { return ProcessPanelBackground(skin.Transparent1); }

  static const QBrush& TimeRulerBackground(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& MinimapBackground(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& TimeRuler(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& LocalTimeRuler(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& SlotHeader(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& IntervalSolidPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalDashPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalRackPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalPlayPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalPlayDashPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalWaitingDashPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalHeaderTextPen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& IntervalBraceSelected(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalBraceWarning(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalBraceInvalid(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& IntervalBrace(const score::Brush& b)
  { return b.main.pen1; }

  static const QBrush& MutedIntervalHeaderBackground(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& IntervalHeaderSeparator(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& FullViewIntervalHeaderSeparator(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& ConditionPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& ConditionTrianglePen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& TimenodePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& TimenodeBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& MinimapPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& MinimapBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush& StateTemporalPointBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QPen& StateTemporalPointPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& StateBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& EventPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& EventBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& TimeRulerLargePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& TimeRulerSmallPen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& SlotHandlePen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& CommentBlockPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& CommentBlockSelectedPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& MiniScenarioPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& SeparatorPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& SeparatorBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& DefaultBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& AudioCablePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& DataCablePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& MidiCablePen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& SelectedAudioCablePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& SelectedDataCablePen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& SelectedMidiCablePen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen& AudioPortPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& DataPortPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen& MidiPortPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush& AudioPortBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& DataPortBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush& MidiPortBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen& SlotHeaderTextPen(const score::Brush& b)
  { return b.main.pen1; }

  static const QPen SlotHeaderPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush SlotHeaderBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen RectPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QPen RectHighlightPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush RectBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QBrush RectHighlightBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QBrush LoopBrush(const score::Brush& b)
  { return b.main.brush; }

  static const QPen TransparentPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush TransparentBrush(const score::Brush& b)
  { return b.main.brush; }
  static const QPen NoPen(const score::Brush& b)
  { return b.main.pen1; }
  static const QBrush NoBrush(const score::Brush& b)
  { return b.main.brush; }

private:
  Style() noexcept;
  void update(const score::Skin& skin);
};
}
