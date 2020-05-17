#pragma once
#include <score/model/ColorReference.hpp>
#include <score/model/Skin.hpp>

#include <QBrush>
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


  const QPen& TransparentPen() const noexcept { return skin.NoPen; }
  const QPen& NoPen() const noexcept { return skin.NoPen; }
  const QBrush& TransparentBrush() const noexcept { return skin.NoBrush; }
  const QBrush& NoBrush() const noexcept { return skin.NoBrush; }

  const score::Brush& IntervalBase() const noexcept { return skin.Base1; }
  const score::Brush& IntervalSelected() const noexcept { return skin.Base2; }
  const score::Brush& IntervalDropTarget() const noexcept { return skin.Warn1; }
  const score::Brush& IntervalPlayFill() const noexcept { return skin.Base3; }
  const score::Brush& IntervalPlayDashFill() const noexcept { return skin.Pulse1; }
  const score::Brush& IntervalWaitingDashFill() const noexcept { return skin.Pulse2; }
  const score::Brush& IntervalLoop() const noexcept { return skin.Warn1; }
  const score::Brush& IntervalWarning() const noexcept { return skin.Warn2; }
  const score::Brush& IntervalInvalid() const noexcept { return skin.Warn3; }
  const score::Brush& IntervalMuted() const noexcept { return skin.HalfDark; }
  const score::Brush& IntervalDefaultLabel() const noexcept { return skin.Gray; }
  const score::Brush& IntervalDefaultBackground() const noexcept { return skin.Transparent1; }

  const score::Brush& RackSideBorder() const noexcept { return skin.Base1; }

  const score::Brush& IntervalFullViewParentSelected() const noexcept { return skin.Emphasis1; }

  const score::Brush& IntervalHeaderText() const noexcept { return skin.Light; }
  const score::Brush& IntervalHeaderBottomLine() const noexcept { return skin.Transparent1; }
  const score::Brush& IntervalHeaderRackHidden() const noexcept { return skin.Transparent1; }
  const score::Brush& IntervalHeaderSideBorder() const noexcept { return skin.Base1; }

  const score::Brush& ProcessViewBorder() const noexcept { return skin.Gray; }

  const score::Brush& SlotFocus() const noexcept { return skin.Base2; }
  const score::Brush& SlotOverlayBorder() const noexcept { return skin.Dark; }
  const score::Brush& SlotOverlay() const noexcept { return skin.Transparent2; }
  const score::Brush& SlotHandle() const noexcept { return skin.Transparent3; }

  const score::Brush& TimenodeDefault() const noexcept { return skin.Gray; }
  const score::Brush& TimenodeSelected() const noexcept { return skin.Base2; }

  const score::Brush& EventDefault() const noexcept { return skin.Emphasis4; }
  const score::Brush& EventWaiting() const noexcept { return skin.HalfLight; }
  const score::Brush& EventPending() const noexcept { return skin.Warn1; }
  const score::Brush& EventHappened() const noexcept { return skin.Base3; }
  const score::Brush& EventDisposed() const noexcept { return skin.Warn3; }
  const score::Brush& EventSelected() const noexcept { return skin.Base2; }

  const score::Brush& ConditionDefault() const noexcept { return skin.Smooth3; }
  const score::Brush& ConditionWaiting() const noexcept { return skin.Gray; }
  const score::Brush& ConditionDisabled() const noexcept { return skin.Base1; }
  const score::Brush& ConditionFalse() const noexcept { return skin.Smooth1; }
  const score::Brush& ConditionTrue() const noexcept { return skin.Smooth2; }

  const score::Brush& StateOutline() const noexcept { return skin.Light; }
  const score::Brush& StateSelected() const noexcept { return skin.Base2; }
  const score::Brush& StateDot() const noexcept { return skin.Base1; }

  const score::Brush& Background() const noexcept { return skin.Background1; }
  const score::Brush& ProcessPanelBackground() const noexcept { return skin.Transparent1; }


  const score::Brush& TimeRulerBackground() const noexcept { return skin.DarkGray; }
  const score::Brush& MinimapBackground() const noexcept { return skin.Background1; }
  const score::Brush& TimeRuler() const noexcept { return skin.Base1; }
  const score::Brush& LocalTimeRuler() const noexcept { return skin.Gray; }

  const score::Brush& SlotHeader() const noexcept { return skin.Base5; }

  const score::Brush& SeparatorBrush() const noexcept { return skin.Base1; }
  const QBrush& SlotHeaderBrush() const noexcept { return skin.NoBrush; }

  // TODO white
  const QPen& CommentBlockPen() const noexcept { return skin.Light.main.pen1; }
  const QPen& CommentBlockSelectedPen() const noexcept { return skin.Light.main.pen2; }

  const QPen& SeparatorPen() const noexcept { return skin.Light.main.pen2; }


  const QPen& RectPen() const noexcept { return skin.Emphasis1.main.pen2_solid_round_round; }
  const QPen& RectHighlightPen() const noexcept { return skin.Emphasis1.lighter.pen2_solid_round_round; }
  const QBrush& RectBrush() const noexcept { return skin.NoBrush; }
  const QBrush& RectHighlightBrush() const noexcept { return skin.NoBrush; }

  static const QPen& IntervalSolidPen(const score::Brush& b) noexcept { return b.main.pen3_solid_flat_miter; }
  static const QPen& IntervalDashPen(const score::Brush& b) noexcept { return b.main.pen3_dashed_flat_miter; }
  static const QPen& IntervalRackPen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }
  static const QPen& IntervalPlayLinePen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }
  const QPen& IntervalHeaderTextPen() const noexcept { return IntervalHeaderText().main.pen1; }

  const QPen& IntervalBrace() const noexcept { return IntervalBase().main.pen2_solid_flat_miter; }
  const QPen& IntervalBraceSelected() const noexcept { return IntervalSelected().main.pen2_solid_flat_miter; }
  const QPen& IntervalBraceWarning() const noexcept { return IntervalWarning().main.pen2_solid_flat_miter; }
  const QPen& IntervalBraceInvalid() const noexcept { return IntervalInvalid().main.pen2_solid_flat_miter; }

  const score::Brush& MutedIntervalHeaderBackground() const noexcept { return skin.HalfDark; }

  const QPen& IntervalHeaderSeparator() const noexcept { return IntervalHeaderSideBorder().main.pen2_solid_flat_miter; }

  static const QPen& ConditionPen(const score::Brush& b) noexcept { return b.main.pen2; }
  static const QPen& ConditionTrianglePen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }

  static const QPen& TimenodePen(const score::Brush& b) noexcept { return b.main.pen2_dotted_square_miter; }

  const QPen& MinimapPen() const noexcept { return skin.LightGray.main.pen_cosmetic; }
  const score::Brush& MinimapBrush() const noexcept { return skin.DarkGray; }


  static const QPen& StateTemporalPointPen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }

  static const QPen& EventPen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }

  const QPen& TimeRulerLargePen() const noexcept { return TimeRuler().main.pen2; }
  const QPen& TimeRulerSmallPen() const noexcept { return TimeRuler().main.pen1; }

  const QPen& SlotHandlePen() const noexcept { return ProcessViewBorder().main.pen0; }

  static const QPen& MiniScenarioPen(const score::Brush& b) noexcept { return b.main.pen_cosmetic; }

  const score::Brush& DefaultBrush() const noexcept { return skin.Base1; }

  const QPen& AudioCablePen() const noexcept { return skin.Cable1.main.pen3_solid_round_round; }
  const QPen& DataCablePen() const noexcept  { return skin.Cable2.main.pen3_solid_round_round; }
  const QPen& MidiCablePen() const noexcept  { return skin.Cable3.main.pen3_solid_round_round; }

  const QPen& SelectedAudioCablePen() const noexcept { return skin.SelectedCable1.lighter.pen3_solid_round_round; }
  const QPen& SelectedDataCablePen() const noexcept  { return skin.SelectedCable2.lighter.pen3_solid_round_round; }
  const QPen& SelectedMidiCablePen() const noexcept  { return skin.SelectedCable3.lighter.pen3_solid_round_round; }

  const QPen& AudioPortPen() const noexcept { return skin.Port1.main.pen1_5; }
  const QPen& DataPortPen() const noexcept  { return skin.Port2.main.pen1_5; }
  const QPen& MidiPortPen() const noexcept  { return skin.Port3.main.pen1_5; }

  const score::BrushSet& AudioPortBrush() const noexcept { return skin.Port1.darker; }
  const score::BrushSet& DataPortBrush() const noexcept  { return skin.Port2.darker; }
  const score::BrushSet& MidiPortBrush() const noexcept  { return skin.Port3.darker; }

  const QPen& SlotHeaderTextPen() const noexcept { return TimenodeDefault().main.pen_cosmetic; }
  const QPen& SlotHeaderPen() const noexcept { return IntervalHeaderSideBorder().main.pen1_solid_flat_miter; }


  const score::Brush& LoopBrush() const noexcept { return skin.Tender3; }
private:
  Style() noexcept;
  ~Style();
  void update(const score::Skin& skin);
};
}
