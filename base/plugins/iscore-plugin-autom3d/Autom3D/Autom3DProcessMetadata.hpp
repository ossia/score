#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <QString>
#include <iscore_plugin_autom3d_export.h>

namespace Autom3D
{
struct ISCORE_PLUGIN_AUTOM3D_EXPORT ProcessMetadata
{
        static const ProcessFactoryKey& factoryKey();

        static QString processObjectName();

        static QString factoryPrettyName();
};
}
