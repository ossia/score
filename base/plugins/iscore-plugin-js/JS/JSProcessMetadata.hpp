#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

namespace JS
{
struct ProcessMetadata
{
        static const ProcessFactoryKey& abstractFactoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
}
