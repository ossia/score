#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct JSProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
