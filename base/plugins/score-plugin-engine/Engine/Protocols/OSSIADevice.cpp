// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <QDebug>
#include <QStringList>
#include <algorithm>
#include <list>

#include "OSSIADevice.hpp"
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/network_logger.hpp>
#include <ossia-qt/name_utils.hpp>
#include <Explorer/DeviceList.hpp>

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
    Device::Node score_device{settings(), nullptr};

    // Recurse on the children
    {
      const auto& ossia_children = dev->get_root_node().children();
      score_device.reserve(ossia_children.size());
      for (const auto& node : ossia_children)
      {
        score_device.push_back(
              Engine::ossia_to_score::ToDeviceExplorer(*node.get()));
      }
    }

    // We change the settings safely
    disconnect();

    m_settings = newsettings;

    if (reconnect())
    {
      // We can recreate our stuff.
      for (const auto& n : score_device.children())
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
    auto& root = dev->get_root_node();
    root.clear_children();
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
    ossia::net::node_base* node = Engine::score_to_ossia::createNodeFromPath(
          settings.address.path, *dev);
    SCORE_ASSERT(node);

    // Populate the node with a parameter (if it isn't a no_value_t).
    Engine::score_to_ossia::createOSSIAAddress(settings, *node);
  }
}

void OSSIADevice::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
  if (auto dev = getDevice())
  {
    ossia::net::node_base* node
        = Engine::score_to_ossia::getNodeFromPath(currentAddr.path, *dev);
    bool is_listening = m_callbacks.find(currentAddr) != m_callbacks.end();

    if (!settings.value.valid())
    {
      if(is_listening)
      {
        // Remove callbacks
        auto it = m_callbacks.find(currentAddr);
        if (it != m_callbacks.end())
        {
          it->second.first->remove_callback(it->second.second);
          m_callbacks.erase(it);
        }

        is_listening = false;
      }

      // Remove param
      node->remove_parameter();
    }
    else
    {
      auto currentAddr = node->get_parameter();
      if (currentAddr)
        Engine::score_to_ossia::updateOSSIAAddress(settings, *currentAddr);
      else
        Engine::score_to_ossia::createOSSIAAddress(settings, *node);
    }

    auto newName = settings.address.path.last();
    if (!latin_compare(newName, node->get_name()))
    {
      renameListening_impl(currentAddr, newName);
      node->set_name(newName.toStdString());
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
    sub_addr.path += QString::fromStdString(child->get_name());
    removeListening_impl(*child.get(), std::move(sub_addr));
  }
}

void OSSIADevice::removeListening_impl(
    ossia::net::node_base& node, State::Address addr, std::vector<State::Address>& vec)
{
  // Find & remove our callback
  auto it = m_callbacks.find(addr);
  if (it != m_callbacks.end())
  {
    it->second.first->remove_callback(it->second.second);
    m_callbacks.erase(it);
    vec.push_back(addr);
  }

  // Recurse
  for (const auto& child : node.children())
  {
    State::Address sub_addr = addr;
    sub_addr.path += QString::fromStdString(child->get_name());
    removeListening_impl(*child.get(), std::move(sub_addr));
  }
}
bool is_parent(const State::Address& parent, const State::Address& child)
{
  const auto p_size = parent.path.size();
  if(child.path.size() < p_size)
    return false;
  for(int i = 0; i < p_size; i++)
  {
    if(child.path[i] != parent.path[i])
      return false;
  }
  return true;
}

void OSSIADevice::renameListening_impl(const State::Address& parent, const QString& newName)
{
  // Store the elements that are renamed
  std::vector<std::pair<State::Address, callback_pair>> saved_elts;
  for(auto it = m_callbacks.begin(); it != m_callbacks.end(); )
  {
    if(is_parent(parent, it.key()))
    {
      State::Address addr = it.key();
      addr.path[parent.path.size() - 1] = newName;
      saved_elts.push_back({std::move(addr), it.value()});
      it = m_callbacks.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Put things back after renaming
  for (auto&& p : std::move(saved_elts))
  {
    m_callbacks.insert(std::move(p));
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
void OSSIADevice::setLogging_impl(DeviceLogging b) const
{
  if (auto dev = getDevice())
  {
    switch (b)
    {
      case DeviceLogging::LogNothing:
      {
        dev->get_protocol().set_logger({});
        return;
      }
      case DeviceLogging::LogEverything:
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
        dev->get_protocol().set_logger(std::move(logger));
        break;
      }
      case DeviceLogging::LogUnfolded:
      {
        ossia::net::network_logger logger;
        logger.inbound_listened_logger = std::make_shared<spdlog::logger>(
              "in_logger", std::make_shared<in_sink>(*this));
        logger.outbound_listened_logger = std::make_shared<spdlog::logger>(
              "out_logger", std::make_shared<out_sink>(*this));

        logger.inbound_listened_logger->set_pattern("%v");
        logger.inbound_listened_logger->set_level(spdlog::level::info);
        logger.outbound_listened_logger->set_pattern("%v");
        logger.outbound_listened_logger->set_level(spdlog::level::info);
        dev->get_protocol().set_logger(std::move(logger));
        break;
      }
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
      dev->on_node_created.connect<OSSIADevice, &OSSIADevice::nodeCreated>(this);
      dev->on_node_removing.connect<OSSIADevice, &OSSIADevice::nodeRemoving>(this);
      dev->on_node_renamed.connect<OSSIADevice, &OSSIADevice::nodeRenamed>(this);
      dev->on_parameter_created.connect<OSSIADevice, &OSSIADevice::addressCreated>(
            this);
      dev->on_parameter_removing.connect<OSSIADevice, &OSSIADevice::addressRemoved>(
            this);
      dev->on_attribute_modified.connect<OSSIADevice, &OSSIADevice::addressUpdated>(
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
      dev->on_node_created.disconnect<OSSIADevice, &OSSIADevice::nodeCreated>(this);
      dev->on_node_removing.disconnect<OSSIADevice, &OSSIADevice::nodeRemoving>(this);
      dev->on_node_renamed.disconnect<OSSIADevice, &OSSIADevice::nodeRenamed>(this);
      dev->on_parameter_created.disconnect<OSSIADevice, &OSSIADevice::addressCreated>(
            this);
      dev->on_parameter_removing.disconnect<OSSIADevice, &OSSIADevice::addressRemoved>(
            this);
      dev->on_attribute_modified.disconnect<OSSIADevice, &OSSIADevice::addressUpdated>(
            this);
    }
    m_callbacksEnabled = false;
  }
}

Device::Node OSSIADevice::simple_refresh()
{
  Device::Node score_device{settings(), nullptr};

  // Recurse on the children
  const auto& ossia_children = getDevice()->get_root_node().children();
  score_device.reserve(ossia_children.size());
  for (const auto& node : ossia_children)
  {
    score_device.push_back(
        Engine::ossia_to_score::ToDeviceExplorer(*node.get()));
  }

  score_device.get<Device::DeviceSettings>().name
      = QString::fromStdString(getDevice()->get_name());

  return score_device;
}

void OSSIADevice::removeNode(const State::Address& address)
{
  using namespace ossia;
  if (!m_capas.canRemoveNode)
    return;
  if (auto dev = getDevice())
  {
    ossia::net::node_base* node
        = Engine::score_to_ossia::getNodeFromPath(address.path, *dev);

    auto parent = node->get_parent();
    if(parent->has_child(*node))
    {
      /* If we are listening to this node, we recursively
       * remove listening to all the children. */
      removeListening_impl(*node, address);

      // TODO !! if we remove nodes while recording
      // (or anything involving a registered listening state), there will be
      // crashes.
      // The Device Explorer should be locked for edition during recording /
      // playing.
      parent->remove_child(node->get_name());
    }
  }
}

Device::Node OSSIADevice::refresh()
{
  Device::Node device_node{settings(), nullptr};

  if (auto dev = getDevice())
  {
    auto& root = dev->get_root_node();
    // Clear the listening
    removeListening_impl(root, State::Address{m_settings.name, {}});

    disableCallbacks();
    m_callbacks.clear();
    if (dev->get_protocol().update(root))
    {
      // Make a device explorer node from the current state of the device.
      // First make the node corresponding to the root node.

      // Recurse on the children
      const auto& children = root.children();
      device_node.reserve(children.size());
      for (const auto& node : children)
      {
        device_node.push_back(
              Engine::ossia_to_score::ToDeviceExplorer(*node.get()));
      }
    }
    enableCallbacks();

    device_node.get<Device::DeviceSettings>().name = settings().name;
  }

  return device_node;
}

optional<ossia::value> OSSIADevice::refresh(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto node = Engine::score_to_ossia::findNodeFromPath(address.path, *dev);
    if (node)
    {
      if (auto addr = node->get_parameter())
      {
        return addr->fetch_value();
      }
    }
  }

  return {};
}

void OSSIADevice::request(const Device::Node& address)
{
  if (auto dev = getDevice())
  {
    auto node = Engine::score_to_ossia::findNodeFromPath(address, *dev);
    if (node)
    {
      if (auto addr = node->get_parameter())
      {
        addr->request_value();
      }
    }
  }
}

Device::Node OSSIADevice::getNode(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto ossia_node
        = Engine::score_to_ossia::findNodeFromPath(address.path, *dev);
    if (ossia_node)
      return Engine::ossia_to_score::ToDeviceExplorer(*ossia_node);
  }

  return {};
}

Device::Node OSSIADevice::getNodeWithoutChildren(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto ossia_node
        = Engine::score_to_ossia::findNodeFromPath(address.path, *dev);
    if (ossia_node)
    {
      return Device::Node{
        Engine::ossia_to_score::ToAddressSettings(*ossia_node), nullptr};
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

    ossia::net::parameter_base* ossia_addr{};
    if (cb_it == m_callbacks.end())
    {
      auto n = Engine::score_to_ossia::findNodeFromPath(addr.path, *dev);
      if (!n)
        return;

      ossia_addr = n->get_parameter();
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

    SCORE_ASSERT(bool(ossia_addr));

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

      valueUpdated(addr, ossia_addr->value());
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
    SCORE_ASSERT(addr.device == this->settings().name);
    setListening(addr, true);
  }
}

void OSSIADevice::sendMessage(const State::Message& mess)
{
  if (auto dev = getDevice())
  {
    if (mess.address.qualifiers.get().accessors.empty())
    {
      auto node = Engine::score_to_ossia::getNodeFromPath(
            mess.address.address.path, *dev);

      auto addr = node->get_parameter();
      if (addr)
      {
        addr->push_value(mess.value);
      }
    }
    else
    {
      SCORE_TODO;
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

void OSSIADevice::setLogging(DeviceLogging b)
{
  if (!connected())
  {
    m_logging = b;
    return;
  }

  if (b == m_logging)
    return;

  m_logging = b;
  setLogging_impl(m_logging);
}

OwningOSSIADevice::~OwningOSSIADevice()
{

}

void OwningOSSIADevice::replaceDevice(ossia::net::device_base* d)
{
  disconnect();
  m_dev.reset(d);
  m_owned = false;
}

void OwningOSSIADevice::releaseDevice()
{
  OSSIADevice::disconnect();
  m_dev.release();
}

void OwningOSSIADevice::disconnect()
{
  if(m_owned)
  {
    OSSIADevice::disconnect();
    m_dev.reset();
  }
}

void OSSIADevice::nodeCreated(const ossia::net::node_base& n)
{
  pathAdded(Engine::ossia_to_score::ToAddress(n));
}

void OSSIADevice::nodeRemoving(const ossia::net::node_base& n)
{
  pathRemoved(Engine::ossia_to_score::ToAddress(n));
}

void OSSIADevice::nodeRenamed(
    const ossia::net::node_base& node, std::string old_name)
{
  if (!node.get_parent())
    return;

  State::Address currentAddress
      = Engine::ossia_to_score::ToAddress(*node.get_parent());
  currentAddress.path.push_back(QString::fromStdString(old_name));

  Device::AddressSettings as
      = Engine::ossia_to_score::ToAddressSettings(node);
  as.name = QString::fromStdString(node.get_name());

  renameListening_impl(currentAddress, as.name);

  pathUpdated(currentAddress, as);
}

void OSSIADevice::addressCreated(const ossia::net::parameter_base& addr)
{
  State::Address currentAddress
      = Engine::ossia_to_score::ToAddress(addr.get_node());
  Device::AddressSettings as
      = Engine::ossia_to_score::ToAddressSettings(addr.get_node());
  pathUpdated(currentAddress, as);
}

void OSSIADevice::addressUpdated(const ossia::net::node_base& node, ossia::string_view key)
{
  const bool hidden = (ossia::net::get_zombie(node) || ossia::net::get_hidden(node));

  State::Address currentAddress
      = Engine::ossia_to_score::ToAddress(node);
  if(hidden)
  {
    pathRemoved(currentAddress);
    return;
  }

  Device::AddressSettings as
      = Engine::ossia_to_score::ToAddressSettings(node);

  pathUpdated(currentAddress, as);
}

void OSSIADevice::addressRemoved(const ossia::net::parameter_base& addr)
{
  auto address = ossia_to_score::ToAddress(addr.get_node());

  auto cb_it = m_callbacks.find(address);
  if(cb_it != m_callbacks.end())
  {
    m_callbacks.erase(cb_it);
  }
  auto& node = addr.get_node();
  State::Address currentAddress
      = Engine::ossia_to_score::ToAddress(node);
  Device::AddressSettings as;
  as.name = QString::fromStdString(node.get_name());
  pathUpdated(currentAddress, as);
}

}
}
