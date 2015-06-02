#pragma once
#include "Process/Temporal/StateMachines/Tools/ScenarioToolState.hpp"
#include <iscore/selection/SelectionDispatcher.hpp>

class ScenarioSelectionState;
class SelectionTool : public ScenarioTool
{
    public:
        SelectionTool(ScenarioStateMachine& sm);

        void on_pressed() override;
        void on_moved() override;
        void on_released() override;

    private:
        ScenarioSelectionState* m_state{};
};
