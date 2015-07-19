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



void recurse_addPaths(DeviceInterface& dev, iscore::Node& node)
{
    auto full = iscore::FullAddressSettings::make(node.addressSettings(), node.parent()->address());

    // Add in the device implementation
    dev.addAddress(full);

    for(auto& child : node.children())
    {
        recurse_addPaths(dev, *child);
    }
}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const VisitorVariant& vis,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{"DeviceDocumentPlugin", parent}
{
    if(vis.identifier == DataStream::type())
    {
        auto& des = static_cast<DataStream::Deserializer&>(vis.visitor);
        des.writeTo(m_rootNode);
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& des = static_cast<JSONObject::Deserializer&>(vis.visitor);
        des.writeTo(m_rootNode);
    }

    // Here we recreate the correct structures in term of devices,
    // given what's present in the node hierarchy
    for(auto& node : m_rootNode.children())
    {
        try {
            // Instantiate a real device.
            auto proto = SingletonProtocolList::instance().protocol(node->deviceSettings().protocol);
            auto newdev = proto->makeDevice(node->deviceSettings());
            m_list.addDevice(newdev);

            for(auto& child : node->children())
            {
                 recurse_addPaths(*newdev, *child);
            }
        }
        catch(std::runtime_error e)
        {
            QMessageBox::warning(QApplication::activeWindow(),
                                 QObject::tr("Error loading device"),
                                 node->deviceSettings().name + ": " + QString::fromLatin1(e.what()));
        }
    }
}

void DeviceDocumentPlugin::serialize(const VisitorVariant& vis) const
{
    // TODO use visitorcommon
    if(vis.identifier == DataStream::type())
    {
        auto& ser = static_cast<DataStream::Serializer&>(vis.visitor);
        ser.readFrom(m_rootNode);
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& ser = static_cast<JSONObject::Serializer&>(vis.visitor);
        ser.readFrom(m_rootNode);
    }
}
