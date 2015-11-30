#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

struct AutomationProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
