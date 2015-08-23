#include "DeviceDocumentPlugin.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"
#include <Singletons/SingletonProtocolList.hpp>

#include <QMessageBox>
#include <QApplication>

DeviceDocumentPlugin::DeviceDocumentPlugin(QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{

}



DeviceDocumentPlugin::DeviceDocumentPlugin(
        const VisitorVariant& vis,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{
    deserialize_dyn(vis, m_rootNode);

    // Here we recreate the correct structures in term of devices,
    // given what's present in the node hierarchy
    for(const auto& node : m_rootNode.children())
    {
        createDeviceFromNode(*node);
    }
}

void DeviceDocumentPlugin::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, m_rootNode);
}

void DeviceDocumentPlugin::createDeviceFromNode(const iscore::Node & node)
{
    try {
        // Instantiate a real device.
        auto proto = SingletonProtocolList::instance().protocol(node.deviceSettings().protocol);
        auto newdev = proto->makeDevice(node.deviceSettings());
        m_list.addDevice(newdev);

        for(const auto& child : node.children())
        {
             addNodeToDevice(*newdev, *child);
        }
    }
    catch(std::runtime_error e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             node.deviceSettings().name + ": " + QString::fromLatin1(e.what()));
    }
}

void DeviceDocumentPlugin::addNodeToDevice(DeviceInterface &dev, iscore::Node &node)
{
    auto full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(node.addressSettings(), iscore::address(*node.parent()));

    // Add in the device implementation
    dev.addAddress(full);

    for(auto& child : node.children())
    {
        addNodeToDevice(dev, *child);
    }
}


