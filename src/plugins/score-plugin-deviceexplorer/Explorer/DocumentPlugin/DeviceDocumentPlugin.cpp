// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceDocumentPlugin.hpp"

#include <State/Address.hpp>

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

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
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

#include <ossia/detail/logger.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/context.hpp>

#include <ossia-qt/invoke.hpp>

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
    const score::DocumentContext& ctx, QObject* parent)
    : score::SerializableDocumentPlugin{ctx, "Explorer::DeviceDocumentPlugin", parent}
{
  init();
  m_explorer = new DeviceExplorerModel{*this, this};
}

void DeviceDocumentPlugin::on_documentClosing()
{
  for(auto dev : this->m_list.devices())
  {
    dev->disconnect();
  }
}

DeviceDocumentPlugin::~DeviceDocumentPlugin()
{
  for(auto dev : this->m_list.devices())
  {
    dev->disconnect();
  }

  m_processMessages = false;
  m_asioContext->context.stop();

#if !defined(__EMSCRIPTEN__)
  m_asioThread.join();
#endif
}

// MOVEME
struct print_node_rec
{
  void visit(const Device::Node& addr)
  {
    qDebug() << Device::address(addr).toString();
    ;
    for(auto& child : addr)
    {
      visit(child);
    }
  }
};

void DeviceDocumentPlugin::init()
{
  m_asioContext = std::make_shared<ossia::net::network_context>();
  m_processMessages = true;
#if defined(__EMSCRIPTEN__)
  startTimer(8);
#else
  m_asioThread = std::thread{[this] {
    ossia::set_thread_name("ossia asio");
    while(m_processMessages)
    {
      m_asioContext->run();
    }
  }};
#endif
}

void DeviceDocumentPlugin::asyncConnect(Device::DeviceInterface& newdev)
{
  const auto w = score::GUIAppContext().mainWindow;
  if(newdev.capabilities().asyncConnect && w)
  {
    w->setEnabled(false);

    QMessageBox b(
        QMessageBox::NoIcon, QString{"Waiting"},
        QString{"Waiting for a device: " + newdev.settings().name},
        QMessageBox::StandardButton::NoButton, w,
        Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    b.setStandardButtons(QMessageBox::StandardButton::Cancel);
    b.setIconPixmap(
        score::get_pixmap(QStringLiteral(":/icons/message_information.png")));

    connect(
        b.button(QMessageBox::StandardButton::Cancel), &QPushButton::clicked, &b,
        [&b] { b.reject(); });

    connect(
        &newdev, &Device::DeviceInterface::connectionChanged, &b, [&b] { b.accept(); });

    // A device that never answers would otherwise hold the dialog, and the
    // whole window with it, for as long as the application runs. Nothing
    // downstream distinguishes "gave up" from "cancelled": both leave the
    // device unconnected, which is a state score already handles.
    constexpr int connectionTimeout = 30000;
    QTimer::singleShot(connectionTimeout, &b, [&b, &newdev] {
      qWarning() << "Gave up waiting for device" << newdev.settings().name;
      b.reject();
    });

    QTimer::singleShot(1, [&] { newdev.reconnect(); });
    b.exec();

    w->setEnabled(true);
  }
  else
  {
    newdev.reconnect();
  }
}

void DeviceDocumentPlugin::timerEvent(QTimerEvent* event)
{
#if defined(__EMSCRIPTEN__)
  if(m_processMessages)
    m_asioContext->poll();
#endif
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
  if(m_context.role() == score::DocumentRole::Terminal)
    return node;

  try
  {
    auto& fact = m_context.app.interfaces<Device::ProtocolFactoryList>();

    // Instantiate a real device.
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    if(!proto)
    {
      // Nothing here can make it. Ordinary rather than exceptional: a session
      // adds devices from whichever machine the user is typing at, and a
      // document routinely names protocols a given build has no factory for.
      // The node is kept, as loading one does -- what is not acceptable is
      // calling through the null.
      qWarning() << "No protocol for device" << node.get<Device::DeviceSettings>().name;
      return node;
    }

    auto newdev
        = proto->makeDevice(node.get<Device::DeviceSettings>(), *this, context());

    if(!newdev)
      throw std::runtime_error("Null device");

    initDevice(*newdev);

    const auto capas = newdev->capabilities();

    // The node's children are user data - learned MIDI or OSC addresses,
    // custom audio nodes, a tree loaded from a .device file, the tree a
    // removed device had when its removal is undone... - whenever the device
    // serializes its tree, and the whole of the tree when it cannot explore
    // its namespace. Put them back in the device first.
    if(capas.canSerialize || !capas.canRefreshTree)
    {
      for(auto& child : node)
      {
        newdev->addNode(child);
      }
    }

    // Then show what the device actually has: for a device which explores a
    // remote namespace (OSCQuery, Minuit...) that is the remote tree; for one
    // which builds its own (audio, MIDI, joystick...) the tree it built, which
    // includes the nodes replayed above.
    if(capas.canRefreshTree)
    {
      auto refreshed = newdev->refresh();

      // Nothing came back - the device is not reachable, or the namespace
      // did not arrive within refresh()'s timeout: keep what we have rather
      // than wiping the tree. It will be explored once the device is
      // reconnected (DeviceDocumentPlugin::reconnect, a manual refresh...).
      if(!refreshed.hasChildren())
        return node;

      return refreshed;
    }

    return node;
  }
  catch(const std::runtime_error& e)
  {
    score::warning(
        QApplication::activeWindow(), QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
  }

  return node;
}

bool DeviceDocumentPlugin::refreshDeviceTree(Device::DeviceInterface& dev)
{
  if(!dev.capabilities().canRefreshTree)
    return false;

  auto refreshed = dev.refresh();

  // An empty answer is indistinguishable from a namespace that did not make
  // it in time: never trade the tree we have for nothing. refresh() dropped
  // the value callbacks though: listen again to what the explorer shows.
  if(!refreshed.hasChildren())
  {
    dev.restoreListening();
    return false;
  }

  // The explorer swaps the device's tree in place and keeps its unfolding,
  // which is what brings the listening back on the nodes it shows; requests
  // made outside of a view (no explorer widget) are restored here.
  const bool replaced = explorer().replaceDevice(std::move(refreshed));
  dev.restoreListening();
  return replaced;
}

void DeviceDocumentPlugin::refreshDeviceTreeOnReconnect(Device::DeviceInterface& dev)
{
  if(!dev.capabilities().canRefreshTree)
    return;

  // Coalesce: undo / redo in quick succession, or several edits, must not
  // pile up refreshes.
  if(!m_pendingTreeRefresh.insert(&dev).second)
    return;

  // deviceChanged(old, new) is emitted by the device once it has been
  // recreated with its new settings - synchronously for some protocols, so
  // this must be hooked before the settings are applied. The device's own
  // handler (DeviceInterface::updateSettings) then replays the previous nodes
  // into the new device; exploring is deferred to run after that replay.
  auto con_handle = std::make_shared<QMetaObject::Connection>();
  *con_handle = connect(
      &dev, &Device::DeviceInterface::deviceChanged, this,
      [this, ptr = QPointer{&dev},
       con_handle](ossia::net::device_base*, ossia::net::device_base* newd) {
    if(!newd)
      return;
    QObject::disconnect(*con_handle);
    if(!ptr)
      return;
    m_pendingTreeRefresh.erase(ptr.data());

    // One deferred exploration at a time per device: a burst of undo / redo on
    // a device that reconnects synchronously must not queue one per step.
    if(!m_queuedTreeRefresh.insert(ptr.data()).second)
      return;

    QMetaObject::invokeMethod(
        this,
        [this, ptr] {
      if(ptr)
        m_queuedTreeRefresh.erase(ptr.data());
      // The device may have been removed in the meantime
      if(ptr && m_list.findDevice(ptr->settings().name) == ptr.data())
        refreshDeviceTree(*ptr);
        },
        Qt::QueuedConnection);
      });

  // If the device goes away before reconnecting, forget about it.
  m_connections[&dev].push_back(*con_handle);
}

std::optional<Device::Node>
DeviceDocumentPlugin::loadDeviceFromNode(const Device::Node& node)
{
  // The score runs on another machine and its devices belong to it: making
  // them here would open that machine's ports, claim its MIDI and cameras, and
  // put its render windows on this screen. The node stays in the tree with
  // nothing behind it, which is the same shape as a protocol we do not have.
  if(m_context.role() == score::DocumentRole::Terminal)
    return {};

  try
  {
    // Instantiate a real device.
    auto& fact = m_context.app.interfaces<Device::ProtocolFactoryList>();
    auto proto = fact.get(node.get<Device::DeviceSettings>().protocol);
    if(!proto)
      throw std::runtime_error("Null protocol");
    Device::DeviceInterface* newdev
        = proto->makeDevice(node.get<Device::DeviceSettings>(), *this, context());

    if(!newdev)
      throw std::runtime_error("Null device");

    initDevice(*newdev);

    // We do not reload for devices such as LocalDevice.
    if(newdev->capabilities().canSerialize)
    {
      for(auto& child : node)
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
  catch(const std::runtime_error& e)
  {
    score::warning(
        QApplication::activeWindow(), QObject::tr("Error loading device"),
        node.get<Device::DeviceSettings>().name + ": " + QString::fromLatin1(e.what()));
  }

  return {};
}

std::optional<bool>
DeviceDocumentPlugin::remoteConnected(const QString& device) const noexcept
{
  if(auto it = m_remoteConnected.find(device); it != m_remoteConnected.end())
    return it->second;
  return {};
}

void DeviceDocumentPlugin::setRemoteConnected(const QString& device, bool connected)
{
  m_remoteConnected[device] = connected;

  // The explorer draws the state, so it has to be told the row changed.
  if(m_explorer)
  {
    auto& root = m_rootNode;
    for(int i = 0; i < root.childCount(); i++)
    {
      const auto& n = root.childAt(i);
      if(n.is<Device::DeviceSettings>()
         && n.get<Device::DeviceSettings>().name == device)
      {
        const auto idx = m_explorer->index(i, 0, QModelIndex{});
        m_explorer->dataChanged(idx, idx);
        break;
      }
    }
  }
}

std::vector<QString>
DeviceDocumentPlugin::remoteDevicesOfKind(Device::DeviceKind kind) const
{
  std::vector<QString> out;
  for(const auto& [name, kinds] : m_remoteKinds)
    if(kinds.testFlag(kind))
      out.push_back(name);
  return out;
}

void DeviceDocumentPlugin::setRemoteKinds(
    const QString& device, Device::DeviceKinds kinds)
{
  m_remoteKinds[device] = kinds;
  remoteKindsChanged(device);
}

void DeviceDocumentPlugin::setConnection(bool b)
{
  if(b)
  {
    // Reconnect all devices
    m_list.apply([&](Device::DeviceInterface& dev) {
      if(!dev.connected())
        asyncConnect(dev);
      if(dev.capabilities().canSerialize)
      {
        auto it = ossia::find_if(m_rootNode, [&](const Device::Node& dev_node) {
          return dev_node.template get<Device::DeviceSettings>().name
                 == dev.settings().name;
        });

        if(it != m_rootNode.cend())
        {
          for(const auto& nodes : *it)
          {
            dev.addNode(nodes);
          }
        }
        else
        {
          qDebug() << "Could not save device";
        }
      }

      // The nodes were just rebuilt: listen again to what the explorer shows
      dev.restoreListening();

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
  if(m_listening)
    return *m_listening;

  m_listening
      = context().app.interfaces<ListeningHandlerFactoryList>().make(*this, context());
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

void DeviceDocumentPlugin::setupConnections(
    Device::DeviceInterface& device, bool enabled)
{
  auto& vec = m_connections[&device];
  if(enabled)
  {
    vec.push_back(
        con(device, &Device::DeviceInterface::pathAdded, this,
            [&, ptr = QPointer{&device}](const State::Address& addr) {
      if(!ptr)
        return;
      // FIXME A subtle bug is introduced if we want to add the root
      // node...
      if(addr.path.size() > 0)
      {
        auto parentAddr = addr;
        parentAddr.path.removeLast();

        Device::Node* parent = Device::try_getNodeFromAddress(m_rootNode, parentAddr);
        if(parent)
        {
          const auto& last = addr.path[addr.path.size() - 1];
          auto it = ossia::find_if(
              *parent, [&](const auto& n) { return n.displayName() == last; });
          if(it == parent->cend())
          {
            updateProxy.addLocalNode(*parent, device.getNodeWithoutChildren(addr));
          }
          else
          {
            // TODO update the node with the new information
          }
        }
      }
    }, Qt::QueuedConnection));

    vec.push_back(con(
        device, &Device::DeviceInterface::pathRemoved, this,
        [&](const State::Address& addr) { updateProxy.removeLocalNode(addr); },
        Qt::QueuedConnection));
    vec.push_back(con(
        device, &Device::DeviceInterface::pathUpdated, this,
        [&](const State::Address& addr, const Device::AddressSettings& set) {
      updateProxy.updateLocalSettings(addr, set, device);
        },
        Qt::QueuedConnection));
  }
  else
  {
    for(auto& q : vec)
    {
      QObject::disconnect(q);
    }
    m_connections.erase(&device);
    m_pendingTreeRefresh.erase(&device);
    m_queuedTreeRefresh.erase(&device);
  }
}

int index(const DeviceDocumentPlugin& model, const QString& name)
{
  for(int i = 0; i < model.rootNode().childCount(); i++)
  {

    const Device::Node& n = model.rootNode().childAt(i);
    if(n.is<Device::DeviceSettings>())
    {
      auto& d = n.get<Device::DeviceSettings>();
      if(d.name == name)
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
      if(device_index != -1)
      {
        auto cmd = new Explorer::Command::ReplaceDevice{
            *this, device_index, std::move(new_node)};

        CommandDispatcher<>{this->context().commandStack}.submit(cmd);
      }
    });
  };

  if(device.isEmpty())
  {
    m_list.apply([&, f](Device::DeviceInterface& dev) {
      if(!dev.connected())
      {
        f(dev);
      }
    });
  }
  else
  {
    m_list.apply([&](Device::DeviceInterface& dev) {
      if(!dev.connected() && dev.settings().name == device)
        f(dev);
    });
  }
}

void DeviceDocumentPlugin::on_valueUpdated(
    const State::Address& addr, const ossia::value& v)
{
  ossia::qt::run_async(this, [this, aa = State::AddressAccessor{addr}, v] {
    updateProxy.updateLocalValue(aa, v);
    if(m_valueObserver)
      m_valueObserver(aa.address, v);
  });
}

}
