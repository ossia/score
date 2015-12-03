#pragma once
#include <iscore/command/CommandFactoryKey.hpp>

namespace iscore
{
class SerializableCommand;
struct CommandData
{
        CommandData() = default;
        explicit CommandData(const iscore::SerializableCommand& cmd);

        CommandParentFactoryKey parentKey;
        CommandFactoryKey commandKey;
        QByteArray data;
};

}
