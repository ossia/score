#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <qstring.h>

struct LoopProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
