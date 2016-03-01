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

// !!! a json file OVERRIDE (potentially) all that !!! (cf ScenarioApplicationPlugin.cpp:initColors()

        const QColor& ConstraintBase;//{3, 195, 221}; //Base1
        const QColor& ConstraintSelected;//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2
        const QColor& ConstraintPlayFill;//{34, 224, 0}; // Base3
        const QColor& ConstraintWarning;//{200,150,0}; //Warn2
        const QColor& ConstraintInvalid;//{Qt::red}; // Warn3
        const QColor& ConstraintDefaultLabel;//{Qt::gray}; // Gray
        const QColor& ConstraintDefaultBackground;//{0, 127, 229, 76}; // Transparent1

        const QColor& RackSideBorder;//{3, 195, 221}; //Base1

        const QColor& ConstraintFullViewParentSelected;//{Qt::cyan}; // Emphasis1

        const QColor& ConstraintHeaderText;//{Qt::white}; // Light
        const QColor& ConstraintHeaderBottomLine;//{0, 127, 229, 76}; // Transparent1
        const QColor& ConstraintHeaderRackHidden;//{0, 127, 229, 76}; // Transparent1
        const QColor& ConstraintHeaderSideBorder;//{3, 195, 221}; // Base1

        const QColor& ProcessViewBorder;//{Qt::gray}; // Gray

        const QColor& SlotFocus;//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2
        const QColor& SlotOverlayBorder;//{Qt::black}; // Dark
        const QColor& SlotOverlay;//{170, 170, 170, 70}; // Transparent2
        const QColor& SlotHandle;//{37, 41, 48, 40}; // Transparent3

        const QColor& TimenodeDefault;//{Qt::darkGray}; // HalfDark
        const QColor& TimenodeSelected;//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2

        const QColor& EventDefault;//{Qt::white}; // Light
        const QColor& EventWaiting;//{Qt::lightGray}; // HalfLight
        const QColor& EventPending;//{Qt::yellow}; // Warn1
        const QColor& EventHappened;//{34, 224, 0}; // Base3
        const QColor& EventDisposed;//{Qt::red};  // Warn3
        const QColor& EventSelected;//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2

        const QColor& ConditionDefault;//{200, 250, 0}; // Smooth3
        const QColor& ConditionWaiting;//{179, 179, 179}; // Gray
        const QColor& ConditionDisabled;//{3, 195, 221}; // Base1
        const QColor& ConditionFalse;//{222, 0, 0}; // Smooth1
        const QColor& ConditionTrue;//{109,222,0}; // Smooth2

        const QColor& StateOutline;//{Qt::white}; // Light
        const QColor& StateSelected;//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2
        const QColor& StateDot;//{3, 195, 221}; // Base1

        const QColor& Background;//{37, 41, 48}; // Background1
        const QColor& ProcessPanelBackground;//{0, 127, 229, 76}; // Transparent1

        const QColor& TimeRulerBackground;//{37, 41, 48}; // Background1
        const QColor& TimeRuler;//{3, 195, 221}; // Base1
        const QColor& LocalTimeRuler;//{179, 179, 179}; // Gray

    private:
        ScenarioStyle() = default;
};
