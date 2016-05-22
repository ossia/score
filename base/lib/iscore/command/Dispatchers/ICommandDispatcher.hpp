#pragma once
namespace iscore
{
    class CommandStackFacade;
}

/**
 * @brief The ICommandDispatcher class
 *
 * Base for command dispatchers.
 * Command dispatchers are utility class to send many commands in a specific
 * way.
 *
 * The general interface is :
 *   - submitCommand(cmd) to send a new command or update an existing one
 *   - commit() when editing is done.
 */
class ICommandDispatcher
{
    public:
        ICommandDispatcher(const iscore::CommandStackFacade& stack):
            m_stack{stack}
        {

        }

        const iscore::CommandStackFacade& stack() const
        {
            return m_stack;
        }

    private:
        const iscore::CommandStackFacade& m_stack;
};
