#pragma once
namespace iscore
{
    class CommandStack;
}

class ICommandDispatcher
{
    public:
        ICommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        iscore::CommandStack& stack() const
        {
            return m_stack;
        }

    private:
        iscore::CommandStack& m_stack;
};
