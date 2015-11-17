#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct LoopProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
