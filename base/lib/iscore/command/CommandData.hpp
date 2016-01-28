#pragma once
#include <iscore/command/CommandFactoryKey.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class SerializableCommand;
struct ISCORE_LIB_BASE_EXPORT CommandData
{
        CommandData() = default;
        explicit CommandData(const iscore::SerializableCommand& cmd);

        CommandParentFactoryKey parentKey;
        CommandFactoryKey commandKey;
        QByteArray data;
};

}
