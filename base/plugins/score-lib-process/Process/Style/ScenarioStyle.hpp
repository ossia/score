#pragma once
#include <QBrush>
#include <QColor>
#include <QPen>
#include <score_lib_process_export.h>
#include <qnamespace.h>

#include <score/model/ColorReference.hpp>
namespace score
{
class Skin;
}

struct SCORE_LIB_PROCESS_EXPORT ScenarioStyle
{
  ScenarioStyle(const score::Skin&) noexcept;

  ScenarioStyle(const ScenarioStyle&) = delete;
  ScenarioStyle(ScenarioStyle&&) = delete;
  ScenarioStyle& operator=(const ScenarioStyle&) = delete;
  ScenarioStyle& operator=(ScenarioStyle&&) = delete;

  void setIntervalWidth(double w);

  static ScenarioStyle& instance();

  score::ColorRef IntervalBase;
  score::ColorRef IntervalSelected;
  score::ColorRef IntervalPlayFill;
  score::ColorRef IntervalPlayDashFill;
  score::ColorRef IntervalWaitingDashFill;
  score::ColorRef IntervalLoop;
  score::ColorRef IntervalWarning;
  score::ColorRef IntervalInvalid;
  score::ColorRef IntervalMuted;
  score::ColorRef IntervalDefaultLabel;
  score::ColorRef IntervalDefaultBackground;

  score::ColorRef RackSideBorder;

  score::ColorRef IntervalFullViewParentSelected;

  score::ColorRef IntervalHeaderText;
  score::ColorRef IntervalHeaderBottomLine;
  score::ColorRef IntervalHeaderRackHidden;
  score::ColorRef IntervalHeaderSideBorder;

  score::ColorRef ProcessViewBorder;

  score::ColorRef SlotFocus;
  score::ColorRef SlotOverlayBorder;
  score::ColorRef SlotOverlay;
  score::ColorRef SlotHandle;

  score::ColorRef TimenodeDefault;
  score::ColorRef TimenodeSelected;

  score::ColorRef EventDefault;
  score::ColorRef EventWaiting;
  score::ColorRef EventPending;
  score::ColorRef EventHappened;
  score::ColorRef EventDisposed;
  score::ColorRef EventSelected;

  score::ColorRef ConditionDefault;
  score::ColorRef ConditionWaiting;
  score::ColorRef ConditionDisabled;
  score::ColorRef ConditionFalse;
  score::ColorRef ConditionTrue;

  score::ColorRef StateOutline;
  score::ColorRef StateSelected;
  score::ColorRef StateDot;

  score::ColorRef Background;
  score::ColorRef ProcessPanelBackground;

  score::ColorRef TimeRulerBackground;
  score::ColorRef TimeRuler;
  score::ColorRef LocalTimeRuler;

  QPen IntervalSolidPen;
  QPen IntervalDashPen;
  QPen IntervalRackPen;
  QPen IntervalPlayPen;
  QPen IntervalPlayDashPen;
  QPen IntervalWaitingDashPen;
  QPen IntervalHeaderTextPen;

  QPen IntervalBraceSelected;
  QPen IntervalBraceWarning;
  QPen IntervalBraceInvalid;
  QPen IntervalBrace;

  QPen IntervalHeaderSeparator;
  QPen FullViewIntervalHeaderSeparator;

  QPen ConditionPen;
  QPen ConditionTrianglePen;

  QPen TimenodePen;
  QBrush TimenodeBrush;

  QPen MinimapPen;
  QBrush MinimapBrush;

  QBrush StateTemporalPointBrush;
  QPen StateTemporalPointPen;
  QBrush StateBrush;

  QPen EventPen;
  QBrush EventBrush;

  QPen TimeRulerLargePen, TimeRulerSmallPen;

  QPen SlotHandlePen;

  QPen TextItemPen;

  QFont Bold10Pt;
  QFont Bold12Pt;
  QFont Medium7Pt;
  QFont Medium8Pt;
  QFont Medium12Pt;

  QPen CommentBlockPen;
  QPen MiniScenarioPen;
  QPen SeparatorPen;
  QBrush SeparatorBrush;
  QBrush DefaultBrush;

  QPen AudioCablePen;
  QPen DataCablePen;
  QPen MidiCablePen;

  QPen SelectedAudioCablePen;
  QPen SelectedDataCablePen;
  QPen SelectedMidiCablePen;

  QPen AudioPortPen;
  QPen DataPortPen;
  QPen MidiPortPen;
  QBrush AudioPortBrush;
  QBrush DataPortBrush;
  QBrush MidiPortBrush;

  QPen GrayTextPen;

  const QPen TransparentPen;
  const QBrush TransparentBrush;
  const QPen NoPen;
  const QBrush NoBrush;
private:
  ScenarioStyle() noexcept;
  void update(const score::Skin& skin);
};
