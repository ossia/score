#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <qstring.h>

struct MappingProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
