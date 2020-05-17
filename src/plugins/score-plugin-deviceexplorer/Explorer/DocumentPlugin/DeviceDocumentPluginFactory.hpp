#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

namespace Explorer
{
class DeviceDocumentPlugin;
using DocumentPluginFactory = score::DocumentPluginFactory_T<DeviceDocumentPlugin>;
}

UUID_METADATA(
    ,
    score::DocumentPluginFactory,
    Explorer::DeviceDocumentPlugin,
    "6e610e1f-9de2-4c36-90dd-0ef570002a21")
