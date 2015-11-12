#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct AutomationProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
