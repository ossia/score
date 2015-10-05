#pragma once
#include <QColor>

struct ScenarioStyle
{
        static ScenarioStyle& instance();

        QColor constraintBase{3, 195, 221}; //Base
        QColor constraintSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        QColor constraintPlayFill{34, 224, 0};
        QColor constraintWarning{200,150,0};
        QColor constraintInvalid{Qt::red};
        QColor constraintDefaultLabel{Qt::gray};
        QColor constraintDefaultBackground{0, 127, 229, 76};

        QColor rackSideBorder{3, 195, 221};

        QColor constraintFullViewParentSelected{Qt::cyan};

        QColor constraintHeaderText{Qt::white};
        QColor constraintHeaderBottomLine{0, 127, 229, 76};
        QColor constraintHeaderRackHidden{0, 127, 229, 76};
        QColor constraintHeaderSideBorder{3, 195, 221};

        QColor processViewBorder{Qt::gray};


        QColor slotFocus{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};
        QColor slotOverlayBorder{Qt::black};
        QColor slotOverlay{170, 170, 170, 70};
        QColor slotHandle{37, 41, 48, 40};

        QColor timenodeDefault{Qt::darkGray};
        QColor timenodeSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor eventDefault{Qt::white};
        QColor eventWaiting{Qt::lightGray};
        QColor eventPending{Qt::yellow};
        QColor eventHappened{34, 224, 0};
        QColor eventDisposed{Qt::red};
        QColor eventSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor conditionWaiting{179, 179, 179}; // AlternateBase
        QColor conditionDisabled{3, 195, 221}; // Base
        QColor conditionFalse{222, 0, 0}; // WindowText
        QColor conditionTrue{109,222,0}; // Button

        QColor stateOutline{Qt::white};
        QColor stateSelected{QColor::fromRgbF(0.188235, 0.54902, 0.776471)};

        QColor background{37, 41, 48};
        QColor processPanelBackground{0, 127, 229, 76};

        QColor timeRulerBackground{37, 41, 48}; // Background
        QColor timeRuler{3, 195, 221};
        QColor localTimeRuler{179, 179, 179};
};
