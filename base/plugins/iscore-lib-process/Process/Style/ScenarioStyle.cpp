#include "ScenarioStyle.hpp"
#include "Skin.hpp"

ScenarioStyle::ScenarioStyle(const Skin& s):

    ConstraintBase{s.Base1},
    ConstraintSelected{s.Base2}, // Base2
    ConstraintPlayFill{s.Base3}, // Base3
    ConstraintWarning{s.Warn2}, //Warn2
    ConstraintInvalid{s.Warn3}, // Warn3
    ConstraintDefaultLabel{s.Gray},//{Qt::gray}; // Gray
    ConstraintDefaultBackground{s.Transparent1},//{0, 127, 229, 76}; // Transparent1

    RackSideBorder{s.Base1},//{3, 195, 221}; //Base1

    ConstraintFullViewParentSelected{s.Emphasis1},//{Qt::cyan}; // Emphasis1

    ConstraintHeaderText{s.Light},//{Qt::white}; // Light
    ConstraintHeaderBottomLine{s.Transparent1},//{0, 127, 229, 76}; // Transparent1
    ConstraintHeaderRackHidden{s.Transparent1},//{0, 127, 229, 76}; // Transparent1
    ConstraintHeaderSideBorder{s.Base1},//{3, 195, 221}; // Base1

    ProcessViewBorder{s.Gray},//{Qt::gray}; // Gray

    SlotFocus{s.Base2},//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2
    SlotOverlayBorder{s.Dark},//{Qt::black}; // Dark
    SlotOverlay{s.Transparent2},//{170, 170, 170, 70}; // Transparent2
    SlotHandle{s.Transparent3},//{37, 41, 48, 40}; // Transparent3

    TimenodeDefault{s.HalfDark},//{Qt::darkGray}; // HalfDark
    TimenodeSelected{s.Base2},//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2

    EventDefault{s.Light},//{Qt::white}; // Light
    EventWaiting{s.HalfLight},//{Qt::lightGray}; // HalfLight
    EventPending{s.Warn1},//{Qt::yellow}; // Warn1
    EventHappened{s.Base3},//{34, 224, 0}; // Base3
    EventDisposed{s.Warn3},//{Qt::red};  // Warn3
    EventSelected{s.Base2},//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2

    ConditionDefault{s.Smooth3},//{200, 250, 0}; // Smooth3
    ConditionWaiting{s.Gray},//{179, 179, 179}; // Gray
    ConditionDisabled{s.Base1},//{3, 195, 221}; // Base1
    ConditionFalse{s.Smooth1},//{222, 0, 0}; // Smooth1
    ConditionTrue{s.Smooth2},//{109,222,0}; // Smooth2

    StateOutline{s.Light},//{Qt::white}; // Light
    StateSelected{s.Base2},//{QColor::fromRgbF(0.188235, 0.54902, 0.776471)}; // Base2
    StateDot{s.Base1},//{3, 195, 221}; // Base1

    Background{s.Background1}, //{37, 41, 48}; // Background1
    ProcessPanelBackground{s.Transparent1},//{0, 127, 229, 76}; // Transparent1

    TimeRulerBackground{s.Background1}, //{37, 41, 48}; // Background1
    TimeRuler{s.Base1},//{3, 195, 221}; // Base1
    LocalTimeRuler{s.Gray}//{179, 179, 179}; // Gray

{

}

ScenarioStyle& ScenarioStyle::instance()
{
    static ScenarioStyle s(Skin::instance());
    return s;
}
