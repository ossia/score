#pragma once
#include "ScenarioCreationState.hpp"
namespace Scenario
{
class Creation_FromState final : public CreationState
{
    public:
        Creation_FromState(
                const ToolPalette& stateMachine,
                const Path<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createToNothing();
        void createToTimeNode();
        void createToEvent();
        void createToState();

        template<typename Fun>
        void creationCheck(Fun&& fun);
};
}
