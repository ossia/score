#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

struct MappingProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
