#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>


#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QObject>

#include <QString>
#include <algorithm>
#include <stdexcept>
#include <vector>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "DeviceDocumentPlugin.hpp"
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <State/Address.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace iscore {
class Document;
struct Value;
}  // namespace iscore
struct VisitorVariant;

DeviceDocumentPlugin::DeviceDocumentPlugin(
        iscore::Document& ctx,
        QObject* parent):
    iscore::DocumentPluginModel{ctx, "DeviceDocumentPlugin", parent}
{

}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        iscore::Document& ctx,
        const VisitorVariant& vis,
        QObject* parent):
    iscore::DocumentPluginModel{ctx, "DeviceDocumentPlugin", parent}
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
        auto& fact = m_context.app.components.factory<DynamicProtocolList>();

        // Instantiate a real device.
        auto proto = fact.list().get(node.get<iscore::DeviceSettings>().protocol);
        auto newdev = proto->makeDevice(node.get<iscore::DeviceSettings>(), context());

        initDevice(*newdev);

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
        auto& fact = m_context.app.components.factory<DynamicProtocolList>();
        auto proto = fact.list().get(node.get<iscore::DeviceSettings>().protocol);
        auto newdev = proto->makeDevice(node.get<iscore::DeviceSettings>(), context());

        initDevice(*newdev);

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

void DeviceDocumentPlugin::initDevice(DeviceInterface& newdev)
{
    con(newdev, &DeviceInterface::valueUpdated,
        this, [&] (const iscore::Address& addr, const iscore::Value& v) {
        updateProxy.updateLocalValue(addr, v);
    });

    con(newdev, &DeviceInterface::pathAdded,
        this, [&] (const iscore::Address& newaddr) {
        auto parentAddr = newaddr;
        parentAddr.path.removeLast();

        auto& parent = iscore::getNodeFromAddress(m_rootNode, parentAddr);
        updateProxy.addLocalNode(
                    parent,
                    newdev.getNode(newaddr));
    });

    con(newdev, &DeviceInterface::pathRemoved,
        this, [&] (const iscore::Address& addr) {
        updateProxy.removeLocalNode(addr);
    });

    con(newdev, &DeviceInterface::pathUpdated,
        this, [&] (
            const iscore::Address& addr,
            const iscore::AddressSettings& set) {
        updateProxy.updateLocalSettings(addr, set);
    });

    m_list.addDevice(&newdev);
    newdev.setParent(this);
}
