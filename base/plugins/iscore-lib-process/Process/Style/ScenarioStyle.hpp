#pragma once
#include <QColor>
#include <qnamespace.h>
#include <iscore_lib_process_export.h>

#include <Process/Style/ColorReference.hpp>
class Skin;
struct ISCORE_LIB_PROCESS_EXPORT ScenarioStyle
{
        ScenarioStyle(const Skin&) noexcept;

        ScenarioStyle(const ScenarioStyle&) = delete;
        ScenarioStyle(ScenarioStyle&&) = delete;
        ScenarioStyle& operator=(const ScenarioStyle&) = delete;
        ScenarioStyle& operator=(ScenarioStyle&&) = delete;

        static ScenarioStyle& instance();

        ColorRef ConstraintBase;
        ColorRef ConstraintSelected;
        ColorRef ConstraintPlayFill;
        ColorRef ConstraintWarning;
        ColorRef ConstraintInvalid;
        ColorRef ConstraintDefaultLabel;
        ColorRef ConstraintDefaultBackground;

        ColorRef RackSideBorder;

        ColorRef ConstraintFullViewParentSelected;

        ColorRef ConstraintHeaderText;
        ColorRef ConstraintHeaderBottomLine;
        ColorRef ConstraintHeaderRackHidden;
        ColorRef ConstraintHeaderSideBorder;

        ColorRef ProcessViewBorder;

        ColorRef SlotFocus;
        ColorRef SlotOverlayBorder;
        ColorRef SlotOverlay;
        ColorRef SlotHandle;

        ColorRef TimenodeDefault;
        ColorRef TimenodeSelected;

        ColorRef EventDefault;
        ColorRef EventWaiting;
        ColorRef EventPending;
        ColorRef EventHappened;
        ColorRef EventDisposed;
        ColorRef EventSelected;

        ColorRef ConditionDefault;
        ColorRef ConditionWaiting;
        ColorRef ConditionDisabled;
        ColorRef ConditionFalse;
        ColorRef ConditionTrue;

        ColorRef StateOutline;
        ColorRef StateSelected;
        ColorRef StateDot;

        ColorRef Background;
        ColorRef ProcessPanelBackground;

        ColorRef TimeRulerBackground;
        ColorRef TimeRuler;
        ColorRef LocalTimeRuler;

    private:
        ScenarioStyle() noexcept = default;
};
