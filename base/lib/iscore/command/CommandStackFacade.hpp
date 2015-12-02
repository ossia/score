#pragma once
#include <core/command/CommandStack.hpp>
namespace iscore
{
class CommandStackFacade
{
    private:
        iscore::CommandStack& m_stack;

    public:
        CommandStackFacade(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        void push(iscore::SerializableCommand* cmd)
        {
            m_stack.push(cmd);
        }

        void redoAndPush(iscore::SerializableCommand* cmd)
        {
            m_stack.redoAndPush(cmd);
        }

        void disableActions()
        {
            m_stack.disableActions();
        }

        void enableActions()
        {
            m_stack.enableActions();
        }
};
}
