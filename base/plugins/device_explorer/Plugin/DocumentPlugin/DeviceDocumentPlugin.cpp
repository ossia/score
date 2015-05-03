#include "DeviceDocumentPlugin.hpp"


DeviceDocumentPlugin::DeviceDocumentPlugin(QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{

}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const VisitorVariant& loader,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{

}

void DeviceDocumentPlugin::serialize(const VisitorVariant&) const
{

}
