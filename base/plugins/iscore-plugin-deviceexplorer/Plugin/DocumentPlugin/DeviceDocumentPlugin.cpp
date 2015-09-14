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
    for(const auto& node : m_rootNode)
    {
        createDeviceFromNode(node);
    }
}

void DeviceDocumentPlugin::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, m_rootNode);
}

iscore::Node DeviceDocumentPlugin::createDeviceFromNode(const iscore::Node & node)
{
    try {
        // Instantiate a real device.
        auto proto = SingletonProtocolList::instance().protocol(node.get<iscore::DeviceSettings>().protocol);
        auto newdev = proto->makeDevice(node.get<iscore::DeviceSettings>());
        connect(newdev, &DeviceInterface::valueUpdated,
                this, [&] (const iscore::Address& addr, const iscore::Value& v) { updateProxy.updateLocalValue(addr, v); });

        m_list.addDevice(newdev);
        newdev->setParent(this);

        if(newdev->canRefresh())
        {
            return newdev->refresh();
        }
        else
        {
            for(auto& child : node)
            {
                addNodeToDevice(*newdev, child);
            }
            return node;
        }
    }
    catch(const std::runtime_error& e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             node.get<iscore::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
    }

    return node;
}

void DeviceDocumentPlugin::addNodeToDevice(DeviceInterface &dev, const iscore::Node& node)
{
    using namespace iscore;
    auto full = FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                    node.get<AddressSettings>(), address(*node.parent()));

    // Add in the device implementation
    dev.addAddress(full);

    for(auto& child : node)
    {
        addNodeToDevice(dev, child);
    }
}


