// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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

#include "DeviceDocumentPlugin.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
#include <State/Address.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <iostream>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Explorer
{
DeviceDocumentPlugin::DeviceDocumentPlugin(
    const score::DocumentContext& ctx, Id<DocumentPlugin> id, QObject* parent)
    : score::SerializableDocumentPlugin{
          ctx, std::move(id), "Explorer::DeviceDocumentPlugin", parent}
{
    m_explorer = new DeviceExplorerModel{*this, this};
}

DeviceDocumentPlugin::~DeviceDocumentPlugin()
{

}

// MOVEME
struct print_node_rec
{
  void visit(const Device::Node& addr)
  {
    std::cerr << Device::address(addr).toString().toStdString() << std::endl;
    for (auto& child : addr)
    {
      visit(child);
    }
  }
};

Device::Node
DeviceDocumentPlugin::createDeviceFromNode(const Device::Node& node)
{
  try
  {
    auto& fact
        = m_context.app.interfaces<Device::ProtocolFactoryList>();

    // Instantiate a real device.
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    auto newdev
        = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

    if (!newdev)
      throw std::runtime_error("Null device");

    initDevice(*newdev);

    if (newdev->capabilities().canRefreshTree)
    {
      return newdev->refresh();
    }
    else
    {
      for (auto& child : node)
      {
        newdev->addNode(child);
      }
      return node;
    }
  }
  catch (const std::runtime_error& e)
  {
    QMessageBox::warning(
        QApplication::activeWindow(),
        QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": "
            + QString::fromLatin1(e.what()));
  }

  return node;
}

optional<Device::Node>
DeviceDocumentPlugin::loadDeviceFromNode(const Device::Node& node)
{
  try
  {
    // Instantiate a real device.
    auto& fact
        = m_context.app.interfaces<Device::ProtocolFactoryList>();
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    Device::DeviceInterface* newdev
        = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

    initDevice(*newdev);

    // We do not reload for devices such as LocalDevice.
    if (newdev->capabilities().canSerialize)
    {
      for (auto& child : node)
      {
        newdev->addNode(child);
      }

      return {};
    }
    else
    {
      // In this case we instead explore the actual
      // device node.
      newdev->reconnect();
      return newdev->refresh();
    }
  }
  catch (const std::runtime_error& e)
  {
    QMessageBox::warning(
        QApplication::activeWindow(),
        QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": "
            + QString::fromLatin1(e.what()));
  }

  return {};
}

void DeviceDocumentPlugin::setConnection(bool b)
{
  if (b)
  {
    m_list.apply([&](Device::DeviceInterface& dev) {
      if (!dev.connected())
        dev.reconnect();
      if (dev.capabilities().canSerialize)
      {
        auto it
            = ossia::find_if(m_rootNode, [&](const Device::Node& dev_node) {
                return dev_node.template get<Device::DeviceSettings>().name
                       == dev.settings().name;
              });

        SCORE_ASSERT(it != m_rootNode.cend());

        for (const auto& nodes : *it)
        {
          dev.addNode(nodes);
        }
      }
    });
  }
  else
  {
    m_list.apply([&](Device::DeviceInterface& dev) { dev.disconnect(); });
  }
}

ListeningHandler& DeviceDocumentPlugin::listening() const
{
  if (m_listening)
    return *m_listening;

  m_listening
      = context().app.interfaces<ListeningHandlerFactoryList>().make(
          *this, context());
  return *m_listening;
}

void DeviceDocumentPlugin::initDevice(Device::DeviceInterface& newdev)
{
  newdev.valueUpdated
      .connect<DeviceDocumentPlugin, &DeviceDocumentPlugin::on_valueUpdated>(
          *this);

  con(newdev, &Device::DeviceInterface::pathAdded, this,
      [&](const State::Address& addr) {
    // FIXME A subtle bug is introduced if we want to add the root node...
    if(addr.path.size() > 0)
    {
      auto parentAddr = addr;
      parentAddr.path.removeLast();

      Device::Node* parent = Device::try_getNodeFromAddress(m_rootNode, parentAddr);
      if (parent)
      {
        const auto& last = addr.path[addr.path.size() - 1];
        auto it = ossia::find_if(*parent, [&] (const auto& n) { return n.displayName() == last; });
        if(it == parent->cend())
        {
          updateProxy.addLocalNode(
                *parent, newdev.getNodeWithoutChildren(addr));
        }
        else
        {
          // TODO update the node with the new information
        }
      }
    }
  });

  con(newdev, &Device::DeviceInterface::pathRemoved, this,
      [&](const State::Address& addr) {
    updateProxy.removeLocalNode(addr);
  });

  con(newdev, &Device::DeviceInterface::pathUpdated, this,
      [&](const State::Address& addr, const Device::AddressSettings& set) {
    updateProxy.updateLocalSettings(addr, set, newdev);
  });

  m_list.addDevice(&newdev);
  newdev.setParent(this);
}

void DeviceDocumentPlugin::on_valueUpdated(
    const State::Address& addr, const ossia::value& v)
{
  updateProxy.updateLocalValue(
      State::AddressAccessor{addr}, v);
}
}
