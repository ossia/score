#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct MappingProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
