// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceDocumentPlugin.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/ReplaceDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
#include <State/Address.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/Pixmap.hpp>
#include <ossia/network/context.hpp>
#include <ossia/detail/logger.hpp>

#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>

#include <wobjectimpl.h>

#include <stdexcept>
#include <vector>
W_OBJECT_IMPL(Explorer::DeviceDocumentPlugin)
namespace Explorer
{
MODEL_METADATA_IMPL_CPP(DeviceDocumentPlugin)
DeviceDocumentPlugin::DeviceDocumentPlugin(
    const score::DocumentContext& ctx,
    Id<DocumentPlugin> id,
    QObject* parent)
    : score::SerializableDocumentPlugin{
        ctx,
        std::move(id),
        "Explorer::DeviceDocumentPlugin",
        parent}
{
  init();
  m_explorer = new DeviceExplorerModel{*this, this};
}

DeviceDocumentPlugin::~DeviceDocumentPlugin()
{
  m_processMessages = false;
  asioContext->context.stop();
  m_asioThread.join();
}

// MOVEME
struct print_node_rec
{
  void visit(const Device::Node& addr)
  {
    qDebug() << Device::address(addr).toString();
    ;
    for (auto& child : addr)
    {
      visit(child);
    }
  }
};

void DeviceDocumentPlugin::init()
{
  asioContext = std::make_shared<ossia::net::network_context>();
  m_processMessages = true;
  m_asioThread = std::thread{
    [this] {
      while(m_processMessages)
      {
        asioContext->run();
      }
    }
  };
}

void DeviceDocumentPlugin::asyncConnect(Device::DeviceInterface& newdev)
{
  const auto w = score::GUIAppContext().mainWindow;
  if (newdev.capabilities().asyncConnect && w)
  {
    w->setEnabled(false);

    QMessageBox b(
        QMessageBox::NoIcon,
        QString{"Waiting"},
        QString{"Waiting for a device: " + newdev.settings().name},
        QMessageBox::StandardButton::NoButton,
        nullptr,
        Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    b.setStandardButtons(QMessageBox::StandardButton::Cancel);
    b.setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_information.png")));

    connect(b.button(QMessageBox::StandardButton::Cancel), &QPushButton::clicked, &b, [&b] {
      b.reject();
    });
    connect(&newdev, &Device::DeviceInterface::connectionChanged, &b, [&b] { b.accept(); });
    newdev.reconnect();
    b.exec();

    w->setEnabled(true);
  }
  else
  {
    newdev.reconnect();
  }
}
/** The following code handles device creation / loading.
 *
 * There are multiple cases:
 * - Adding a new device:
 *   - Creating a device that won't have anything set-up (Default OSC device)
 *   - Creating a device with a pre-existing node (loading an OSC device through a device file)
 *   - Creating a device that needs refreshing (OSCQuery, Minuit, MIDI...)
 * - Loading a save file:
 *   - Same cases than above. When refreshing however the previous node is kept
 *     in case we want to work without e.g. a device available
 *   -> what happens for not found MIDI devices
 *   -> what happens for not found OSCQuery devices
 */

Device::Node DeviceDocumentPlugin::createDeviceFromNode(const Device::Node& node)
{
  try
  {
    auto& fact = m_context.app.interfaces<Device::ProtocolFactoryList>();

    // Instantiate a real device.
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    auto newdev = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

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
    score::warning(
        QApplication::activeWindow(),
        QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
  }

  return node;
}

std::optional<Device::Node> DeviceDocumentPlugin::loadDeviceFromNode(
      const Device::Node& node)
{
  try
  {
    // Instantiate a real device.
    auto& fact = m_context.app.interfaces<Device::ProtocolFactoryList>();
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    Device::DeviceInterface* newdev
        = proto->makeDevice(node.get<Device::DeviceSettings>(), context());

    if (!newdev)
      throw std::runtime_error("Null device");

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
      return newdev->refresh();
    }
  }
  catch (const std::runtime_error& e)
  {
    score::warning(
        QApplication::activeWindow(),
        QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
  }

  return {};
}

void DeviceDocumentPlugin::setConnection(bool b)
{
  if (b)
  {
    // Reconnect all devices
    m_list.apply([&](Device::DeviceInterface& dev) {
      if (!dev.connected())
        asyncConnect(dev);
      if (dev.capabilities().canSerialize)
      {
        auto it = ossia::find_if(m_rootNode, [&](const Device::Node& dev_node) {
          return dev_node.template get<Device::DeviceSettings>().name == dev.settings().name;
        });

        if (it != m_rootNode.cend())
        {
          for (const auto& nodes : *it)
          {
            dev.addNode(nodes);
          }
        }
        else
        {
          qDebug() << "Could not save device";
        }
      }

      setupConnections(dev, true);
    });
  }
  else
  {
    // Disconnect all devices
    m_list.apply([&](Device::DeviceInterface& dev) {
      setupConnections(dev, false);
      dev.disconnect();
    });
  }
}

ListeningHandler& DeviceDocumentPlugin::listening() const
{
  if (m_listening)
    return *m_listening;

  m_listening = context().app.interfaces<ListeningHandlerFactoryList>().make(*this, context());
  return *m_listening;
}

void DeviceDocumentPlugin::initDevice(Device::DeviceInterface& newdev)
{
  asyncConnect(newdev);
  newdev.valueUpdated.connect<&DeviceDocumentPlugin::on_valueUpdated>(*this);

  setupConnections(newdev, true);

  m_list.addDevice(&newdev);
  newdev.setParent(this);
}

void DeviceDocumentPlugin::setupConnections(Device::DeviceInterface& device, bool enabled)
{
  auto& vec = m_connections[&device];
  if (enabled)
  {
    vec.push_back(
        con(device, &Device::DeviceInterface::pathAdded, this, [&](const State::Address& addr) {
          // FIXME A subtle bug is introduced if we want to add the root
          // node...
          if (addr.path.size() > 0)
          {
            auto parentAddr = addr;
            parentAddr.path.removeLast();

            Device::Node* parent = Device::try_getNodeFromAddress(m_rootNode, parentAddr);
            if (parent)
            {
              const auto& last = addr.path[addr.path.size() - 1];
              auto it = ossia::find_if(
                  *parent, [&](const auto& n) { return n.displayName() == last; });
              if (it == parent->cend())
              {
                updateProxy.addLocalNode(*parent, device.getNodeWithoutChildren(addr));
              }
              else
              {
                // TODO update the node with the new information
              }
            }
          }
        }));

    vec.push_back(con(
        device,
        &Device::DeviceInterface::pathRemoved,
        this,
        [&](const State::Address& addr) { updateProxy.removeLocalNode(addr); },
        Qt::QueuedConnection));
    vec.push_back(con(
        device,
        &Device::DeviceInterface::pathUpdated,
        this,
        [&](const State::Address& addr, const Device::AddressSettings& set) {
          updateProxy.updateLocalSettings(addr, set, device);
        },
        Qt::QueuedConnection));
  }
  else
  {
    for (auto& q : vec)
    {
      QObject::disconnect(q);
    }
    m_connections.erase(&device);
  }
}

int index(const DeviceDocumentPlugin& model, const QString& name)
{
  for (int i = 0; i < model.rootNode().childCount(); i++)
  {

    const Device::Node& n = model.rootNode().childAt(i);
    if (n.is<Device::DeviceSettings>())
    {
      auto& d = n.get<Device::DeviceSettings>();
      if (d.name == name)
        return i;
    }
  }
  return -1;
}
void DeviceDocumentPlugin::reconnect(const QString& device)
{
  auto f = [this](Device::DeviceInterface& dev) {
    dev.reconnect();
    QTimer::singleShot(500, [this, &dev] {
      auto new_node = dev.refresh();

      auto device_index = index(*this, dev.settings().name);
      if (device_index != -1)
      {
        auto cmd = new Explorer::Command::ReplaceDevice{*this, device_index, std::move(new_node)};

        CommandDispatcher<>{this->context().commandStack}.submit(cmd);
      }
    });
  };

  if (device.isEmpty())
  {
    m_list.apply([&, f](Device::DeviceInterface& dev) {
      if (!dev.connected())
      {
        f(dev);
      }
    });
  }
  else
  {
    m_list.apply([&](Device::DeviceInterface& dev) {
      if (!dev.connected() && dev.settings().name == device)
        f(dev);
    });
  }
}

void DeviceDocumentPlugin::on_valueUpdated(const State::Address& addr, const ossia::value& v)
{
  updateProxy.updateLocalValue(State::AddressAccessor{addr}, v);
}

}
