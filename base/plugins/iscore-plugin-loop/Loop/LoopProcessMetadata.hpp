#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

struct LoopProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
