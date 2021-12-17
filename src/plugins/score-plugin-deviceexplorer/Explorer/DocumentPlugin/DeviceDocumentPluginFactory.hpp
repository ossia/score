#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

namespace Explorer
{
class DeviceDocumentPlugin;
using DocumentPluginFactory
    = score::DocumentPluginFactory_T<DeviceDocumentPlugin>;
}
