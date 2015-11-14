#pragma once
#include "ScenarioCreationState.hpp"

namespace Scenario
{
class Creation_FromTimeNode final : public CreationState
{
    public:
        Creation_FromTimeNode(
                const ToolPalette& stateMachine,
                const Path<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialEventAndState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
}
