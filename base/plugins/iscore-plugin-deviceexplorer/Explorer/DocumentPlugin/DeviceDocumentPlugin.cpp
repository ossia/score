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

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
#include <State/Address.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Explorer
{
DeviceDocumentPlugin::DeviceDocumentPlugin(
        const iscore::DocumentContext& ctx,
        QObject* parent):
    iscore::SerializableDocumentPlugin{ctx, "Explorer::DeviceDocumentPlugin", parent}
{
}

DeviceDocumentPlugin::DeviceDocumentPlugin(
        const iscore::DocumentContext& ctx,
        const VisitorVariant& vis,
        QObject* parent):
    iscore::SerializableDocumentPlugin{ctx, "Explorer::DeviceDocumentPlugin", parent}
{
    deserialize_dyn(vis, *this);

    // Here we recreate the correct structures in term of devices,
    // given what's present in the node hierarchy
    for(const auto& node : m_rootNode)
    {
        loadDeviceFromNode(node);
    }
}

void DeviceDocumentPlugin::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

auto DeviceDocumentPlugin::concreteFactoryKey() const -> ConcreteFactoryKey
{
    return DocumentPluginFactory::static_concreteFactoryKey();
}

Device::Node DeviceDocumentPlugin::createDeviceFromNode(const Device::Node & node)
{
    try {
        auto& fact = m_context.app.components.factory<Device::DynamicProtocolList>();

        // Instantiate a real device.
        auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
        auto newdev = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

        initDevice(*newdev);

        if(newdev->capabilities().canRefreshTree)
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
                             node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
    }

    return node;
}

Device::Node DeviceDocumentPlugin::loadDeviceFromNode(const Device::Node & node)
{
    try {
        // Instantiate a real device.
        auto& fact = m_context.app.components.factory<Device::DynamicProtocolList>();
        auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
        Device::DeviceInterface* newdev = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

        initDevice(*newdev);

        // We do not reload for devices such as LocalDevice.
        if(newdev->capabilities().canSerialize)
        {
            for(auto& child : node)
            {
                newdev->addNode(child);
            }
        }

        return node;
    }
    catch(const std::runtime_error& e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
    }

    return node;
}

void DeviceDocumentPlugin::setConnection(bool b)
{
    for(auto& dev : m_list.devices())
    {
        if(b)
        {
            dev->reconnect();
            auto it = std::find_if(m_rootNode.cbegin(), m_rootNode.cend(), [&,dev] (const auto& dev_node) {
                return dev_node.template get<Device::DeviceSettings>().name == dev->settings().name;
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

ListeningHandler &DeviceDocumentPlugin::listening() const
{
    if(m_listening)
        return *m_listening;

    m_listening = context().app.components.factory<ListeningHandlerFactoryList>().make(*this, context());
    return *m_listening;
}

void DeviceDocumentPlugin::initDevice(Device::DeviceInterface& newdev)
{
    con(newdev, &Device::DeviceInterface::valueUpdated,
        this, [&] (const State::Address& addr, const State::Value& v) {
        updateProxy.updateLocalValue(addr, v);
    });

    con(newdev, &Device::DeviceInterface::pathAdded,
        this, [&] (const State::Address& newaddr) {
        auto parentAddr = newaddr;
        parentAddr.path.removeLast();

        auto parent = Device::try_getNodeFromAddress(m_rootNode, parentAddr);
        if(parent)
        {
            updateProxy.addLocalNode(
                        *parent,
                        newdev.getNode(newaddr));
        }
    });

    con(newdev, &Device::DeviceInterface::pathRemoved,
        this, [&] (const State::Address& addr) {
        updateProxy.removeLocalNode(addr);
    });

    con(newdev, &Device::DeviceInterface::pathUpdated,
        this, [&] (
            const State::Address& addr,
            const Device::AddressSettings& set) {
        updateProxy.updateLocalSettings(addr, set);
    });

    m_list.addDevice(&newdev);
    newdev.setParent(this);
}
}
