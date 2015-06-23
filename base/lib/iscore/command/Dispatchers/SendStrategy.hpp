#pragma once
#include <core/command/CommandStack.hpp>

namespace SendStrategy
{
    struct Simple
    {
        static void send(iscore::CommandStack& cmd,
                         iscore::SerializableCommand* other)
        {
            cmd.redoAndPush(other);
        }
    };

    struct Quiet
    {
        static void send(iscore::CommandStack& cmd,
                         iscore::SerializableCommand* other)
        {
            cmd.push(other);
        }
    };
}
