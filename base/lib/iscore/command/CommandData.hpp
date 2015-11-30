#pragma once
#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
struct CommandData
{
        CommandData() = default;
        explicit CommandData(const iscore::SerializableCommand& cmd):
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
