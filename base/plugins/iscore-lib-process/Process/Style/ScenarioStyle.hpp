#pragma once

#include <QColor>
#include <qnamespace.h>
#include <iscore_lib_process_export.h>
struct Skin;
struct ISCORE_LIB_PROCESS_EXPORT ScenarioStyle
{
        ScenarioStyle(const Skin&);

        ScenarioStyle(const ScenarioStyle&) = delete;
        ScenarioStyle(ScenarioStyle&&) = delete;
        ScenarioStyle& operator=(const ScenarioStyle&) = delete;
        ScenarioStyle& operator=(ScenarioStyle&&) = delete;

        static ScenarioStyle& instance();

        const QColor& ConstraintBase;
        const QColor& ConstraintSelected;
        const QColor& ConstraintPlayFill;
        const QColor& ConstraintWarning;
        const QColor& ConstraintInvalid;
        const QColor& ConstraintDefaultLabel;
        const QColor& ConstraintDefaultBackground;

        const QColor& RackSideBorder;

        const QColor& ConstraintFullViewParentSelected;

        const QColor& ConstraintHeaderText;
        const QColor& ConstraintHeaderBottomLine;
        const QColor& ConstraintHeaderRackHidden;
        const QColor& ConstraintHeaderSideBorder;

        const QColor& ProcessViewBorder;

        const QColor& SlotFocus;
        const QColor& SlotOverlayBorder;
        const QColor& SlotOverlay;
        const QColor& SlotHandle;

        const QColor& TimenodeDefault;
        const QColor& TimenodeSelected;

        const QColor& EventDefault;
        const QColor& EventWaiting;
        const QColor& EventPending;
        const QColor& EventHappened;
        const QColor& EventDisposed;
        const QColor& EventSelected;

        const QColor& ConditionDefault;
        const QColor& ConditionWaiting;
        const QColor& ConditionDisabled;
        const QColor& ConditionFalse;
        const QColor& ConditionTrue;

        const QColor& StateOutline;
        const QColor& StateSelected;
        const QColor& StateDot;

        const QColor& Background;
        const QColor& ProcessPanelBackground;

        const QColor& TimeRulerBackground;
        const QColor& TimeRuler;
        const QColor& LocalTimeRuler;

    private:
        ScenarioStyle() = default;
};
