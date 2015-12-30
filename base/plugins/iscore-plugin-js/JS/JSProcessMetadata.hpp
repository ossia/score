#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>

namespace JS
{
struct ProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
}
