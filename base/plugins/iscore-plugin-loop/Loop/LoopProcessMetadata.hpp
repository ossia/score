#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

namespace Loop
{
struct ProcessMetadata
{
        static const ProcessFactoryKey& abstractFactoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
}
