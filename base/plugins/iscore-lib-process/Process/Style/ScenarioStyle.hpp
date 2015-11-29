#pragma once

#include <qcolor.h>
#include <qnamespace.h>

struct ScenarioStyle
{
        ScenarioStyle(const ScenarioStyle&) = delete;
        ScenarioStyle(ScenarioStyle&&) = delete;
        ScenarioStyle& operator=(const ScenarioStyle&) = delete;
        ScenarioStyle& operator=(ScenarioStyle&&) = delete;

        static ScenarioStyle& instance();

        QColor ConstraintBase{3, 195, 221}; //Base
        QColor ConstraintSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        QColor ConstraintPlayFill{34, 224, 0};
        QColor ConstraintWarning{200,150,0};
        QColor ConstraintInvalid{Qt::red};
        QColor ConstraintDefaultLabel{Qt::gray};
        QColor ConstraintDefaultBackground{0, 127, 229, 76};

        QColor RackSideBorder{3, 195, 221};

        QColor ConstraintFullViewParentSelected{Qt::cyan};

        QColor ConstraintHeaderText{Qt::white};
        QColor ConstraintHeaderBottomLine{0, 127, 229, 76};
        QColor ConstraintHeaderRackHidden{0, 127, 229, 76};
        QColor ConstraintHeaderSideBorder{3, 195, 221};

        QColor ProcessViewBorder{Qt::gray};

        QColor SlotFocus{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        QColor SlotOverlayBorder{Qt::black};
        QColor SlotOverlay{170, 170, 170, 70};
        QColor SlotHandle{37, 41, 48, 40};

        QColor TimenodeDefault{Qt::darkGray};
        QColor TimenodeSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor EventDefault{Qt::white};
        QColor EventWaiting{Qt::lightGray};
        QColor EventPending{Qt::yellow};
        QColor EventHappened{34, 224, 0};
        QColor EventDisposed{Qt::red};
        QColor EventSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor ConditionWaiting{179, 179, 179}; // AlternateBase
        QColor ConditionDisabled{3, 195, 221}; // Base
        QColor ConditionFalse{222, 0, 0}; // WindowText
        QColor ConditionTrue{109,222,0}; // Button

        QColor StateOutline{Qt::white};
        QColor StateSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor Background{37, 41, 48};
        QColor ProcessPanelBackground{0, 127, 229, 76};

        QColor TimeRulerBackground{37, 41, 48}; // Background
        QColor TimeRuler{3, 195, 221};
        QColor LocalTimeRuler{179, 179, 179};

    private:
        ScenarioStyle() = default;
};
