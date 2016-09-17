#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

namespace Explorer
{
class DeviceDocumentPlugin;
using DocumentPluginFactory = iscore::DocumentPluginFactory_T<DeviceDocumentPlugin>;
}


UUID_METADATA(,
    iscore::DocumentPluginFactory,
    Explorer::DeviceDocumentPlugin,
    "6e610e1f-9de2-4c36-90dd-0ef570002a21")
