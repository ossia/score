#pragma once
#include <iscore_lib_process_export.h>
#include <QFont>
#include <QColor>

struct ISCORE_LIB_PROCESS_EXPORT Skin
{
        QFont SansFont;
        QFont MonoFont;

        QColor Dark{Qt::black};
        QColor HalfDark{Qt::darkGray};
        QColor Gray{Qt::gray};
        QColor HalfLight{Qt::lightGray};
        QColor Light{Qt::white};

        QColor Emphasis1{Qt::cyan};
        QColor Emphasis2{233, 208, 89};
        QColor Emphasis3{179, 90, 209};

        QColor Base1{3, 195, 221}; //Base
        QColor Base2{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Selected
        QColor Base3{34, 224, 0}; // Playing
        QColor Base4{179, 179, 179}; // Playing

        QColor Warn1{Qt::yellow}; // Yellow : inform, Condition pending
        QColor Warn2{200,150,0}; // Orange : warning
        QColor Warn3{Qt::red}; // Red : error

        QColor Background1{37, 41, 48}; // ConstraintDefaultBackground

        QColor Transparent1{0, 127, 229, 76}; // ConstraintDefaultBackground
        QColor Transparent2{170, 170, 170, 70}; // SlotOverlay
        QColor Transparent3{37, 41, 48, 40}; // SlotHandle

        QColor Smooth1{222, 0, 0};
        QColor Smooth2{109,222,0};
        QColor Smooth3{200, 250, 0};

        QColor Tender1{199, 31, 44};
        QColor Tender2{216, 178, 24};
        QColor Tender3{128, 215, 62};
        // RackSideBorder -> Base1

        // ConstraintDefaultLabel -> gray
        // Emphasis1 QColor ConstraintFullViewParentSelected{Qt::cyan};

        // Light QColor ConstraintHeaderText{Qt::white}; // Light
        // Background1 QColor ConstraintHeaderBottomLine{0, 127, 229, 76};
        // Background1 QColor ConstraintHeaderRackHidden{0, 127, 229, 76};
        // Base1 QColor ConstraintHeaderSideBorder{3, 195, 221};

        // Text1 QColor ProcessViewBorder{Qt::gray};

        // Base2 QColor SlotFocus{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        // Dark QColor SlotOverlayBorder{Qt::black};

        // HalfDark QColor TimenodeDefault{Qt::darkGray};
        // Base2 QColor TimenodeSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        // Light QColor EventDefault{Qt::white};
        // HalfLight QColor EventWaiting{Qt::lightGray};
        // Error1 QColor EventPending{Qt::yellow};
        // Base3 QColor EventHappened{34, 224, 0};
        // Warn3 QColor EventDisposed{Qt::red};
        // Base2 QColor EventSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        // Smooth3 QColor ConditionDefault{200, 250, 0};
        // Base4 QColor ConditionWaiting{179, 179, 179}; // AlternateBase
        // Base QColor ConditionDisabled{3, 195, 221}; // Base
        // Smooth1 QColor ConditionFalse{222, 0, 0}; // WindowText
        // Smooth2 QColor ConditionTrue{109,222,0}; // Button

        // Light QColor StateOutline{Qt::white};
        // Base2 QColor StateSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        // Base1 QColor StateDot{3, 195, 221};

        // Background1 QColor Background{37, 41, 48};
        // Transparent1 QColor ProcessPanelBackground{0, 127, 229, 76};

        // Background1 QColor TimeRulerBackground{37, 41, 48}; // Background
        // Base1 QColor TimeRuler{3, 195, 221};
        // Base4 QColor LocalTimeRuler{179, 179, 179};

};
