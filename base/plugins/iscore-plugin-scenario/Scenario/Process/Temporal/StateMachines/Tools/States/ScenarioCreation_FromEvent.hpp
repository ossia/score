#pragma once
#include "ScenarioCreationState.hpp"
namespace Scenario
{
class Creation_FromEvent final : public CreationState
{
    public:
        Creation_FromEvent(
                const ToolPalette& stateMachine,
                const Path<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
}
