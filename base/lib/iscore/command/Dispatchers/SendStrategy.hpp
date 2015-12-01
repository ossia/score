#pragma once
#include <iscore/command/CommandStackFacade.hpp>

namespace SendStrategy
{
    struct Simple
    {
        static void send(iscore::CommandStackFacade& stack,
                         iscore::SerializableCommand* other)
        {
            stack.redoAndPush(other);
        }
    };

    struct Quiet
    {
        static void send(iscore::CommandStackFacade& stack,
                         iscore::SerializableCommand* other)
        {
            stack.push(other);
        }
    };

    struct UndoRedo
    {
        static void send(iscore::CommandStackFacade& stack,
                         iscore::SerializableCommand* other)
        {
            other->undo();
            stack.redoAndPush(other);
        }
    };
}
namespace RedoStrategy
{
    struct Redo
    {
        static void redo(iscore::SerializableCommand& cmd)
        {
            cmd.redo();
        }
    };

    struct Quiet
    {
        static void redo(iscore::SerializableCommand& cmd)
        {
        }
    };
}

