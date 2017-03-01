#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <list>

#include "OSSIADevice.hpp"
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/network_logger.hpp>
#include <boost/timer.hpp>

namespace Engine
{
namespace Network
{
OSSIADevice::~OSSIADevice()
{
}

bool OSSIADevice::connected() const
{
  return bool(getDevice());
}

void OSSIADevice::updateSettings(const Device::DeviceSettings& newsettings)
{
  // TODO we have to maintain the prior connection state
  // if we were disconnected, we stay disconnected
  // else we reconnect. See in Minuit / MIDI also.
  if (auto dev = getDevice())
  {
    // First we save the existing nodes.
    Device::Node iscore_device{settings(), nullptr};

    // Recurse on the children
    {
      const auto& ossia_children = dev->getRootNode().children();
      iscore_device.reserve(ossia_children.size());
      for (const auto& node : ossia_children)
      {
        iscore_device.push_back(
              Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
      }
    }

    // We change the settings safely
    disconnect();

    m_settings = newsettings;

    if (reconnect())
    {
      // We can recreate our stuff.
      for (const auto& n : iscore_device.children())
      {
        addNode(n);
      }
    }
  }
  else
  {
    // We're already disconnected
    m_settings = newsettings;
  }
}

void OSSIADevice::disconnect()
{
  if(m_capas.hasCallbacks)
    disableCallbacks();

  m_callbacks.clear();
  if (auto dev = getDevice())
  {
    auto& root = dev->getRootNode();
    root.clearChildren();
  }
}

void OSSIADevice::addAddress(const Device::FullAddressSettings& settings)
{
  using namespace ossia;
  if (!m_capas.canAddNode)
    return; // TODO return bool instead, and check in the node update proxy ?

  if (auto dev = getDevice())
  {
    // Create the node. It is added into the device.
    ossia::net::node_base* node = Engine::iscore_to_ossia::createNodeFromPath(
          settings.address.path, *dev);
    ISCORE_ASSERT(node);

    // Populate the node with an address (if it isn't a no_value_t).
    Engine::iscore_to_ossia::createOSSIAAddress(settings, *node);
  }
}

void OSSIADevice::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
  if (auto dev = getDevice())
  {
    ossia::net::node_base* node
        = Engine::iscore_to_ossia::getNodeFromPath(currentAddr.path, *dev);
    auto newName = settings.address.path.last().toStdString();
    if (newName != node->getName())
    {
      node->setName(newName);
    }

    if (settings.value.val.which() == State::ValueType::NoValue)
    {
      node->removeAddress();
    }
    else
    {
      auto currentAddr = node->getAddress();
      if (currentAddr)
        Engine::iscore_to_ossia::updateOSSIAAddress(settings, *currentAddr);
      else
        Engine::iscore_to_ossia::createOSSIAAddress(settings, *node);
    }
  }
}

void OSSIADevice::removeListening_impl(
    ossia::net::node_base& node, State::Address addr)
{
  // Find & remove our callback
  auto it = m_callbacks.find(addr);
  if (it != m_callbacks.end())
  {
    it->second.first->remove_callback(it->second.second);
    m_callbacks.erase(it);
  }

  // Recurse
  for (const auto& child : node.children())
  {
    State::Address sub_addr = addr;
    sub_addr.path += QString::fromStdString(child->getName());
    removeListening_impl(*child.get(), std::move(sub_addr));
  }
}

namespace
{
struct in_sink final : public spdlog::sinks::sink
{
  const OSSIADevice& m_dev;
  in_sink(const OSSIADevice& dev) : m_dev{dev}
  {
  }
  void log(const spdlog::details::log_msg& msg) override
  {
    m_dev.logInbound(
          QString::fromLatin1(msg.formatted.data(), msg.formatted.size()));
  }

  void flush() override
  {
  }
};
struct out_sink final : public spdlog::sinks::sink
{
  const OSSIADevice& m_dev;
  out_sink(const OSSIADevice& dev) : m_dev{dev}
  {
  }
  void log(const spdlog::details::log_msg& msg) override
  {
    m_dev.logOutbound(
          QString::fromLatin1(msg.formatted.data(), msg.formatted.size()));
  }

  void flush() override
  {
  }
};
}
void OSSIADevice::setLogging_impl(bool b) const
{
  if (auto dev = getDevice())
  {
    if (b)
    {
      ossia::net::network_logger logger;
      logger.inbound_logger = std::make_shared<spdlog::logger>(
            "in_logger", std::make_shared<in_sink>(*this));
      logger.outbound_logger = std::make_shared<spdlog::logger>(
            "out_logger", std::make_shared<out_sink>(*this));

      logger.inbound_logger->set_pattern("%v");
      logger.inbound_logger->set_level(spdlog::level::info);
      logger.outbound_logger->set_pattern("%v");
      logger.outbound_logger->set_level(spdlog::level::info);
      dev->getProtocol().setLogger(std::move(logger));
    }
    else
    {
      dev->getProtocol().setLogger({});
    }
  }
}

void OSSIADevice::enableCallbacks()
{
  if(!m_callbacksEnabled)
  {
    auto dev = getDevice();
    if(dev)
    {
      dev->onNodeCreated.connect<OSSIADevice, &OSSIADevice::nodeCreated>(this);
      dev->onNodeRemoving.connect<OSSIADevice, &OSSIADevice::nodeRemoving>(this);
      dev->onNodeRenamed.connect<OSSIADevice, &OSSIADevice::nodeRenamed>(this);
      dev->onAddressCreated.connect<OSSIADevice, &OSSIADevice::addressCreated>(
            this);
      dev->onAttributeModified.connect<OSSIADevice, &OSSIADevice::addressUpdated>(
            this);
    }
    m_callbacksEnabled = true;
  }
}

void OSSIADevice::disableCallbacks()
{
  if(m_callbacksEnabled)
  {
    auto dev = getDevice();
    if(dev)
    {
      dev->onNodeCreated.disconnect<OSSIADevice, &OSSIADevice::nodeCreated>(this);
      dev->onNodeRemoving.disconnect<OSSIADevice, &OSSIADevice::nodeRemoving>(this);
      dev->onNodeRenamed.disconnect<OSSIADevice, &OSSIADevice::nodeRenamed>(this);
      dev->onAddressCreated.disconnect<OSSIADevice, &OSSIADevice::addressCreated>(
            this);
      dev->onAttributeModified.disconnect<OSSIADevice, &OSSIADevice::addressUpdated>(
            this);
    }
    m_callbacksEnabled = false;
  }
}

void OSSIADevice::removeNode(const State::Address& address)
{
  using namespace ossia;
  if (!m_capas.canRemoveNode)
    return;
  if (auto dev = getDevice())
  {
    ossia::net::node_base* node
        = Engine::iscore_to_ossia::getNodeFromPath(address.path, *dev);

    auto parent = node->getParent();
    if(parent->hasChild(*node))
    {
      /* If we are listening to this node, we recursively
       * remove listening to all the children. */
      removeListening_impl(*node, address);

      // TODO !! if we remove nodes while recording
      // (or anything involving a registered listening state), there will be
      // crashes.
      // The Device Explorer should be locked for edition during recording /
      // playing.
      parent->removeChild(node->getName());
    }
  }
}

Device::Node OSSIADevice::refresh()
{
  Device::Node device_node{settings(), nullptr};

  if (auto dev = getDevice())
  {
    auto& root = dev->getRootNode();
    // Clear the listening
    removeListening_impl(root, State::Address{m_settings.name, {}});

    disableCallbacks();
    if (dev->getProtocol().update(root))
    {
      // Make a device explorer node from the current state of the device.
      // First make the node corresponding to the root node.

      // Recurse on the children
      const auto& children = root.children();
      device_node.reserve(children.size());
      for (const auto& node : children)
      {
        device_node.push_back(
              Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
      }
    }
    enableCallbacks();

    device_node.get<Device::DeviceSettings>().name = settings().name;
  }

  return device_node;
}

optional<State::Value> OSSIADevice::refresh(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto node = Engine::iscore_to_ossia::findNodeFromPath(address.path, *dev);
    if (node)
    {
      if (auto addr = node->getAddress())
      {
        addr->pullValue();
        return State::fromOSSIAValue(addr->cloneValue());
      }
    }
  }

  return {};
}

void OSSIADevice::request(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto node = Engine::iscore_to_ossia::findNodeFromPath(address.path, *dev);
    if (node)
    {
      if (auto addr = node->getAddress())
      {
        addr->requestValue();
      }
    }
  }
}

Device::Node OSSIADevice::getNode(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto ossia_node
        = Engine::iscore_to_ossia::findNodeFromPath(address.path, *dev);
    if (ossia_node)
      return Engine::ossia_to_iscore::ToDeviceExplorer(*ossia_node);
  }

  return {};
}

Device::Node OSSIADevice::getNodeWithoutChildren(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto ossia_node
        = Engine::iscore_to_ossia::findNodeFromPath(address.path, *dev);
    if (ossia_node)
    {
      return Device::Node{
        Engine::ossia_to_iscore::ToAddressSettings(*ossia_node), nullptr};
    }
  }

  return {};
}

void OSSIADevice::setListening(const State::Address& addr, bool b)
{
  if (auto dev = getDevice())
  {
    // First check if the address is already listening
    // so that we don't have to go through the tree.
    auto cb_it = m_callbacks.find(addr);

    ossia::net::address_base* ossia_addr{};
    if (cb_it == m_callbacks.end())
    {
      auto n = Engine::iscore_to_ossia::findNodeFromPath(addr.path, *dev);
      if (!n)
        return;

      ossia_addr = n->getAddress();
      if (!ossia_addr)
        return;
    }
    else
    {
      ossia_addr = cb_it->second.first;
      if (!ossia_addr)
      {
        m_callbacks.erase(cb_it);
        return;
      }
    }

    ISCORE_ASSERT(bool(ossia_addr));

    // If we want to enable listening
    // and the address wasn't already listening
    if (b)
    {
      if (cb_it == m_callbacks.end())
      {
        m_callbacks.insert({addr,
                            {ossia_addr, ossia_addr->add_callback(
                             [=](const ossia::value& val) {
                               valueUpdated(addr, val);
                             })}});
      }

      valueUpdated(addr, ossia_addr->cloneValue());
    }
    else
    {
      // If we can disable listening
      if (cb_it != m_callbacks.end())
      {
        ossia_addr->remove_callback(cb_it->second.second);
        m_callbacks.erase(cb_it);
      }
    }
  }
}

std::vector<State::Address> OSSIADevice::listening() const
{
  if (!connected())
    return {};

  std::vector<State::Address> addrs;
  addrs.reserve(m_callbacks.size());

  for (const auto& elt : m_callbacks)
  {
    addrs.push_back(elt.first);
  }

  return addrs;
}

void OSSIADevice::addToListening(const std::vector<State::Address>& addresses)
{
  if (!connected())
    return;

  for (const auto& addr : addresses)
  {
    ISCORE_ASSERT(addr.device == this->settings().name);
    setListening(addr, true);
  }
}

void OSSIADevice::sendMessage(const State::Message& mess)
{
  if (auto dev = getDevice())
  {
    if (mess.address.qualifiers.get().accessors.empty())
    {
      auto node = Engine::iscore_to_ossia::getNodeFromPath(
            mess.address.address.path, *dev);

      auto addr = node->getAddress();
      if (addr)
      {
        addr->pushValue(Engine::iscore_to_ossia::toOSSIAValue(mess.value));
      }
    }
    else
    {
      ISCORE_TODO;
      // FIXME handle address accessor
      // We have to get the current value, and merge the accessed element
      // inside.
      // See for instance the various value merging algorithms in ossia.
    }
  }
}

bool OSSIADevice::isLogging() const
{
  return m_logging;
}

void OSSIADevice::setLogging(bool b)
{
  if (!connected())
    return;

  if (b == m_logging)
    return;

  m_logging = b;
  setLogging_impl(m_logging);
}

void OwningOSSIADevice::disconnect()
{
  OSSIADevice::disconnect();
  m_dev.reset();
}

void OSSIADevice::nodeCreated(const ossia::net::node_base& n)
{
  emit pathAdded(Engine::ossia_to_iscore::ToAddress(n));
}

void OSSIADevice::nodeRemoving(const ossia::net::node_base& n)
{
  emit pathRemoved(Engine::ossia_to_iscore::ToAddress(n));
}

void OSSIADevice::nodeRenamed(
    const ossia::net::node_base& node, std::string old_name)
{
  if (!node.getParent())
    return;

  State::Address currentAddress
      = Engine::ossia_to_iscore::ToAddress(*node.getParent());
  currentAddress.path.push_back(QString::fromStdString(old_name));

  Device::AddressSettings as
      = Engine::ossia_to_iscore::ToAddressSettings(node);
  as.name = QString::fromStdString(node.getName());
  emit pathUpdated(currentAddress, as);
}

void OSSIADevice::addressCreated(const ossia::net::address_base& addr)
{
  State::Address currentAddress
      = Engine::ossia_to_iscore::ToAddress(addr.getNode());
  Device::AddressSettings as
      = Engine::ossia_to_iscore::ToAddressSettings(addr.getNode());
  emit pathUpdated(currentAddress, as);
}

void OSSIADevice::addressUpdated(const ossia::net::node_base& node, ossia::string_view key)
{
  State::Address currentAddress
      = Engine::ossia_to_iscore::ToAddress(node);
  Device::AddressSettings as
      = Engine::ossia_to_iscore::ToAddressSettings(node);
  emit pathUpdated(currentAddress, as);
}
}
}
