#pragma once
#include <core/command/CommandStack.hpp>
class ScenarioModel;
class ScenarioGlobalCommandManager
{
    public:
        ScenarioGlobalCommandManager(iscore::CommandStack& stack):
            m_commandStack{stack}
        {

        }

        void deleteSelection(const ScenarioModel &scenario);
        void clearContentFromSelection(const ScenarioModel &scenario);

    private:
        iscore::CommandStack& m_commandStack;
};
