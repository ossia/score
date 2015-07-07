#include "DeviceDocumentPlugin.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

DeviceDocumentPlugin::DeviceDocumentPlugin(QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{

}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const VisitorVariant& loader,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{
    // Note : we should maybe have a button instead "reinitiate connections" that will fetch the data
    // from the DeviceExplorer's Node hierarchy and rebuild here ?
    //deserialize_dyn(loader, *this);
}

void DeviceDocumentPlugin::serialize(const VisitorVariant& vis) const
{
    //serialize_dyn(vis, *this);
}
