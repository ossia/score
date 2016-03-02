#include "ScenarioStyle.hpp"
#include "Skin.hpp"

ScenarioStyle::ScenarioStyle(const Skin& s):

    ConstraintBase{&s.Base1},
    ConstraintSelected{&s.Base2},
    ConstraintPlayFill{&s.Base3},
    ConstraintWarning{&s.Warn2},
    ConstraintInvalid{&s.Warn3},
    ConstraintDefaultLabel{&s.Gray},
    ConstraintDefaultBackground{&s.Transparent1},

    RackSideBorder{&s.Base1},

    ConstraintFullViewParentSelected{&s.Emphasis1},

    ConstraintHeaderText{&s.Light},
    ConstraintHeaderBottomLine{&s.Transparent1},
    ConstraintHeaderRackHidden{&s.Transparent1},
    ConstraintHeaderSideBorder{&s.Base1},

    ProcessViewBorder{&s.Gray},

    SlotFocus{&s.Base2},
    SlotOverlayBorder{&s.Dark},
    SlotOverlay{&s.Transparent2},
    SlotHandle{&s.Transparent3},

    TimenodeDefault{&s.HalfDark},
    TimenodeSelected{&s.Base2},

    EventDefault{&s.Emphasis4},
    EventWaiting{&s.HalfLight},
    EventPending{&s.Warn1},
    EventHappened{&s.Base3},
    EventDisposed{&s.Warn3},
    EventSelected{&s.Base2},

    ConditionDefault{&s.Smooth3},
    ConditionWaiting{&s.Gray},
    ConditionDisabled{&s.Base1},
    ConditionFalse{&s.Smooth1},
    ConditionTrue{&s.Smooth2},

    StateOutline{&s.Light},
    StateSelected{&s.Base2},
    StateDot{&s.Base1},

    Background{&s.Background1},
    ProcessPanelBackground{&s.Transparent1},

    TimeRulerBackground{&s.Background1},
    TimeRuler{&s.Base1},
    LocalTimeRuler{&s.Gray}
{

}

ScenarioStyle& ScenarioStyle::instance()
{
    static ScenarioStyle s(Skin::instance());
    return s;
}
