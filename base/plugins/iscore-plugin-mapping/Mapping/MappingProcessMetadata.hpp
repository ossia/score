#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

namespace Mapping
{
struct MappingProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
}
