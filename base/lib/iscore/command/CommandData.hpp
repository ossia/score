#pragma once
#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
struct CommandData
{
        CommandData() = default;
        explicit CommandData(iscore::SerializableCommand& cmd):
            parentKey{cmd.parentKey()},
            commandKey{cmd.key()},
            data{cmd.serialize()}
        {

        }

        CommandParentFactoryKey parentKey;
        CommandFactoryKey commandKey;
        QByteArray data;
};

}
