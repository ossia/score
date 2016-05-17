#pragma once


namespace Scenario
{
    // SPace at the left of the main box in the main scenario view.
    static const constexpr int ScenarioLeftSpace = 0; // -5
    static const constexpr int ConstraintHeaderHeight = 30;

    class ItemType{
        public:
            enum Type {Constraint = 1, LeftBrace, RightBrace, SlotHandle, SlotOverlay, ConstraintHeader, TimeNode, Trigger, Event, State, Comment};
    };

    class ZPos{
        public:
        enum ItemZPos {Comment = 1, TimeNode, Event ,Constraint , State};
        enum ConstraintItemZPos {Header= 1, Rack, Brace};
    };
}
