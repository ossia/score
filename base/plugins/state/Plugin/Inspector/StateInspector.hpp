#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <State/State.hpp>

class StateInspector : public InspectorWidgetBase
{
    public:
        StateInspector(const State&, QObject* parent);

};
