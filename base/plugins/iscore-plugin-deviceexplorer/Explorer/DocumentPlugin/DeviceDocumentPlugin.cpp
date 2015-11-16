#include "DeviceDocumentPlugin.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/SingletonProtocolList.hpp>

#include <core/application/ApplicationComponents.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <QMessageBox>
#include <QApplication>

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const iscore::DocumentContext& ctx,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{ctx, "DeviceDocumentPlugin", parent}
{

}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const iscore::DocumentContext& ctx,
        const VisitorVariant& vis,
        QObject* parent):
    iscore::DocumentDelegatePluginModel{ctx, "DeviceDocumentPlugin", parent}
{
    deserialize_dyn(vis, m_rootNode);

    // Here we recreate the correct structures in term of devices,
    // given what's present in the node hierarchy
    for(const auto& node : m_rootNode)
    {
        loadDeviceFromNode(node);
    }
}

void DeviceDocumentPlugin::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, m_rootNode);
}

iscore::Node DeviceDocumentPlugin::createDeviceFromNode(const iscore::Node & node)
{
    try {
        auto fact = m_context.app.components.factory<DynamicProtocolList>();
        ISCORE_ASSERT(fact);

        // Instantiate a real device.
        auto proto = fact->list().get(node.get<iscore::DeviceSettings>().protocol);
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
                newdev->addNode(child);
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

iscore::Node DeviceDocumentPlugin::loadDeviceFromNode(const iscore::Node & node)
{
    try {
        // Instantiate a real device.
        auto fact = m_context.app.components.factory<DynamicProtocolList>();
        ISCORE_ASSERT(fact);

        // Instantiate a real device.
        auto proto = fact->list().get(node.get<iscore::DeviceSettings>().protocol);
        auto newdev = proto->makeDevice(node.get<iscore::DeviceSettings>());
        connect(newdev, &DeviceInterface::valueUpdated,
                this, [&] (const iscore::Address& addr, const iscore::Value& v) { updateProxy.updateLocalValue(addr, v); });

        m_list.addDevice(newdev);
        newdev->setParent(this);
        for(auto& child : node)
        {
            newdev->addNode(child);
        }

        return node;
    }
    catch(const std::runtime_error& e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             node.get<iscore::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
    }

    return node;
}






ListeningState DeviceDocumentPlugin::pauseListening()
{
    ListeningState l;
    for(auto device : m_list.devices())
    {
        auto vec = device->listening();
        for(const auto& elt : vec)
            device->setListening(elt, false);

        l.listened.push_back(std::move(vec));
    }

    // Note : here we do not prevent new listening connections
    // to be created. Else for instance recording would not work.

    // The clients have to take care to not open new listening connections.
    // Maybe find something better for this ?

    return l;
}

void DeviceDocumentPlugin::resumeListening(const ListeningState& st)
{
    for(const auto& vec : st.listened)
    {
        if(vec.empty())
            continue;

        auto& dev = m_list.device(vec.front().device);
        dev.replaceListening(vec);
    }
}

void DeviceDocumentPlugin::setConnection(bool b)
{
    for(auto& dev : m_list.devices())
    {
        if(b)
        {
            dev->reconnect();
            auto it = std::find_if(m_rootNode.cbegin(), m_rootNode.cend(), [&] (const auto& dev_node) {
                return dev_node.template get<iscore::DeviceSettings>().name == dev->settings().name;
            });

            ISCORE_ASSERT(it != m_rootNode.cend());

            for(const auto& nodes : *it)
            {
                dev->addNode(nodes);
            }
        }
        else
            dev->disconnect();
    }
}
