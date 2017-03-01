#pragma once
#include <QBrush>
#include <QColor>
#include <QPen>
#include <iscore_lib_process_export.h>
#include <qnamespace.h>

#include <iscore/model/ColorReference.hpp>
namespace iscore
{
class Skin;
}

struct ISCORE_LIB_PROCESS_EXPORT ScenarioStyle
{
  ScenarioStyle(const iscore::Skin&) noexcept;

  ScenarioStyle(const ScenarioStyle&) = delete;
  ScenarioStyle(ScenarioStyle&&) = delete;
  ScenarioStyle& operator=(const ScenarioStyle&) = delete;
  ScenarioStyle& operator=(ScenarioStyle&&) = delete;

  void setConstraintWidth(double w);

  static ScenarioStyle& instance();

  iscore::ColorRef ConstraintBase;
  iscore::ColorRef ConstraintSelected;
  iscore::ColorRef ConstraintPlayFill;
  iscore::ColorRef ConstraintPlayDashFill;
  iscore::ColorRef ConstraintLoop;
  iscore::ColorRef ConstraintWarning;
  iscore::ColorRef ConstraintInvalid;
  iscore::ColorRef ConstraintMuted;
  iscore::ColorRef ConstraintDefaultLabel;
  iscore::ColorRef ConstraintDefaultBackground;

  iscore::ColorRef RackSideBorder;

  iscore::ColorRef ConstraintFullViewParentSelected;

  iscore::ColorRef ConstraintHeaderText;
  iscore::ColorRef ConstraintHeaderBottomLine;
  iscore::ColorRef ConstraintHeaderRackHidden;
  iscore::ColorRef ConstraintHeaderSideBorder;

  iscore::ColorRef ProcessViewBorder;

  iscore::ColorRef SlotFocus;
  iscore::ColorRef SlotOverlayBorder;
  iscore::ColorRef SlotOverlay;
  iscore::ColorRef SlotHandle;

  iscore::ColorRef TimenodeDefault;
  iscore::ColorRef TimenodeSelected;

  iscore::ColorRef EventDefault;
  iscore::ColorRef EventWaiting;
  iscore::ColorRef EventPending;
  iscore::ColorRef EventHappened;
  iscore::ColorRef EventDisposed;
  iscore::ColorRef EventSelected;

  iscore::ColorRef ConditionDefault;
  iscore::ColorRef ConditionWaiting;
  iscore::ColorRef ConditionDisabled;
  iscore::ColorRef ConditionFalse;
  iscore::ColorRef ConditionTrue;

  iscore::ColorRef StateOutline;
  iscore::ColorRef StateSelected;
  iscore::ColorRef StateDot;

  iscore::ColorRef Background;
  iscore::ColorRef ProcessPanelBackground;

  iscore::ColorRef TimeRulerBackground;
  iscore::ColorRef TimeRuler;
  iscore::ColorRef LocalTimeRuler;

  QPen ConstraintSolidPen;
  QPen ConstraintDashPen;
  QPen ConstraintRackPen;
  QPen ConstraintPlayPen;
  QPen ConstraintPlayDashPen;

  QPen TimenodePen;
  QBrush TimenodeBrush;

  QBrush StateTemporalPointBrush;
  QBrush StateBrush;

  QPen EventPen;
  QBrush EventBrush;

  QPen TransparentPen;

  QPen TimeRulerLargePen, TimeRulerSmallPen;

private:
  ScenarioStyle() noexcept;
  void update();
};
