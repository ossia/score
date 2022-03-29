// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceInterface.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <score/tools/std/String.hpp>

#include <ossia-qt/name_utils.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/network_logger.hpp>
#include <ossia/network/context.hpp>
#include <spdlog/sinks/sink.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/post.hpp>

#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Device::DeviceInterface)
namespace Device
{
struct DeviceSettings;

static void
ToAddress_rec(State::Address& addr, const ossia::net::node_base* cur, int N)
{
  if (auto padre = cur->get_parent())
  {
    ToAddress_rec(addr, padre, N + 1);
    addr.path.push_back(QString::fromStdString(cur->get_name()));
  }
  else
  {
    // The last node is the root node "/", which by convention
    // has the same name than the device
    addr.path.reserve(N);
    addr.device = QString::fromStdString(cur->get_name());
  }
}

static State::Address ToAddress(const ossia::net::node_base& node)
{
  State::Address addr;
  const ossia::net::node_base* cur = &node;

  ToAddress_rec(addr, cur, 1);
  return addr;
}

ossia::net::node_base*
findNodeFromPath(const QStringList& path, ossia::net::device_base& dev)
{
  using namespace ossia;
  // Find the relevant node to add in the device
  ossia::net::node_base* node = &dev.get_root_node();
  for (int i = 0; i < path.size(); i++)
  {
    auto cld = node->find_child(path[i]);
    if (cld)
      node = cld;
    else
    {
      qDebug() << "looking for" << path
               << " -- last found: " << node->get_name() << "\n";
      return {};
    }
  }

  return node;
}

using small_node_vec = ossia::small_vector<const Device::Node*, 16>;
static void getPath(small_node_vec& v, const Device::Node* cur)
{
  auto p = cur->parent();
  if (p && !p->is<Device::DeviceSettings>())
  {
    getPath(v, p);
  }
  v.push_back(cur);
}

ossia::net::node_base*
findNodeFromPath(const Device::Node& path, ossia::net::device_base& dev)
{
  using namespace ossia;
  // Find the relevant node to add in the device
  ossia::net::node_base* node = &dev.get_root_node();
  if (!path.is<Device::DeviceSettings>())
  {
    // First fill the vector of nodes
    ossia::small_vector<const Device::Node*, 16> vec;
    getPath(vec, &path);

    for (std::size_t i = 0; i < vec.size(); i++)
    {
      auto cld = node->find_child(vec[i]->displayName());
      if (cld)
        node = cld;
      else
      {
        qDebug() << "looking for" << Device::address(path).address << " " << i
                 << " " << vec.size() << " " << vec[i]->displayName()
                 << " -- last found: " << node->get_name() << "\n";
        return {};
      }
    }
    return node;
  }
  else
  {
    return &dev.get_root_node();
  }
}

static Device::AddressSettings
ToAddressSettings(const ossia::net::node_base& node)
{
  Device::AddressSettings s;
  const auto& addr = node.get_parameter();

  if (addr)
  {
    //addr->request_value();

    s.name = QString::fromStdString(node.get_name());
    s.ioType = ossia::access_mode::BI; // addr->get_access();
    s.clipMode = addr->get_bounding();
    s.repetitionFilter = addr->get_repetition_filter();
    s.unit = addr->get_unit();
    s.extendedAttributes = node.get_extended_attributes();
    s.domain = addr->get_domain();

    try
    {
      s.value = addr->value();
    }
    catch (...)
    {
      s.value = ossia::init_value(addr->get_value_type());
    }
  }
  else
  {
    s.name = QString::fromStdString(node.get_name());
  }
  return s;
}

static void updateOSSIAAddress(
    const Device::FullAddressSettings& settings,
    ossia::net::parameter_base& addr)
{
  SCORE_ASSERT(settings.ioType);
  addr.set_access(*settings.ioType);

  addr.set_bounding(settings.clipMode);

  addr.set_repetition_filter(
      ossia::repetition_filter(settings.repetitionFilter));

  addr.set_value_type(settings.value.get_type());

  addr.set_value(settings.value);

  addr.set_domain(settings.domain);

  addr.set_unit(settings.unit);

  addr.get_node().set_extended_attributes(settings.extendedAttributes);
}

static void createOSSIAAddress(
    const Device::FullAddressSettings& settings,
    ossia::net::node_base& node)
{
  if (!settings.value.v)
    return;

  auto addr = node.create_parameter(settings.value.get_type());
  if (addr)
    updateOSSIAAddress(settings, *addr);
}

Device::Node ToDeviceExplorer(const ossia::net::node_base& ossia_node)
{
  Device::Node score_node{ToAddressSettings(ossia_node), nullptr};
  {
    const auto& cld = ossia_node.children();
    score_node.reserve(cld.size());

    // 2. Recurse on the children
    for (const auto& ossia_child : cld)
    {
      if (!ossia::net::get_hidden(*ossia_child)
          && !ossia::net::get_zombie(*ossia_child))
      {
        auto child_n = ToDeviceExplorer(*ossia_child);
        child_n.setParent(&score_node);
        score_node.push_back(std::move(child_n));
      }
    }
  }
  return score_node;
}

ossia::net::node_base*
createNodeFromPath(const QStringList& path, ossia::net::device_base& dev)
{
  using namespace ossia;
  // Find the relevant node to add in the device
  ossia::net::node_base* node = &dev.get_root_node();
  for (int i = 0; i < path.size(); i++)
  {
    auto cld = node->find_child(path[i]);
    if (!cld)
    {
      // We have to start adding sub-nodes from here.
      ossia::net::node_base* parentnode = node;
      for (int k = i; k < path.size(); k++)
      {
        auto path_k = path[k].toStdString();
        auto newNode = parentnode->create_child(path_k);
        SCORE_ASSERT(newNode);
        if (path_k != newNode->get_name())
        {
          qDebug() << "Warning! " << path[k]
                   << " was requested as node name but backend converted to  "
                   << newNode->get_name().c_str()
                   << "was obtained. Check for special characters.";
          parentnode->remove_child(*newNode);
          return nullptr;
          /*
          for (const auto& node : parentnode->children())
          {
            qDebug() << node->get_name().c_str();
          }*/
        }
        if (k == path.size() - 1)
        {
          node = newNode;
        }
        else
        {
          parentnode = newNode;
        }
      }

      break;
    }
    else
    {
      node = cld;
    }
  }

  return node;
}

bool DeviceInterface::connected() const
{
  return bool(getDevice());
}

void DeviceInterface::addAddress(const FullAddressSettings& settings)
{
  using namespace ossia;
  if (!m_capas.canAddNode)
  {
    return; // TODO return bool instead, and check in the node update proxy ?
  }

  if (auto dev = getDevice())
  {
    // Create the node. It is added into the device.
    ossia::net::node_base* node
        = createNodeFromPath(settings.address.path, *dev);
    if (node)
    {
      // Populate the node with a parameter (if it isn't a no_value_t).
      createOSSIAAddress(settings, *node);
    }
  }
}

bool DeviceInterface::isLearning() const
{
  return false;
}

void DeviceInterface::setLearning(bool) { }

QMimeData* DeviceInterface::mimeData() const
{
  return nullptr;
}

void DeviceInterface::setupContextMenu(QMenu&) const { }

DeviceInterface::DeviceInterface(Device::DeviceSettings s)
    : m_settings(std::move(s))
{
}

DeviceInterface::~DeviceInterface() { }

const Device::DeviceSettings& DeviceInterface::settings() const
{
  return m_settings;
}

const QString& DeviceInterface::name() const
{
  return settings().name;
}

void DeviceInterface::addNode(const Device::Node& n)
{
  auto full = Device::FullAddressSettings::make<
      Device::FullAddressSettings::as_parent>(
      n.get<Device::AddressSettings>(), Device::address(*n.parent()));

  // Add in the device implementation
  addAddress(full);

  for (const auto& child : n)
  {
    addNode(child);
  }
}

DeviceCapas DeviceInterface::capabilities() const
{
  return m_capas;
}

void DeviceInterface::disconnect()
{
  if (m_capas.hasCallbacks)
    disableCallbacks();

  m_callbacks.clear();
  if (auto dev = getDevice())
  {
    auto& root = dev->get_root_node();
    root.clear_children();
  }
}

void DeviceInterface::recreate(const Device::Node&) { }

void DeviceInterface::updateSettings(const Device::DeviceSettings& newsettings)
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
        score_device.push_back(ToDeviceExplorer(*node.get()));
      }
    }

    // We change the settings safely
    disconnect();

    m_settings = newsettings;

    reconnect();

    auto con_handle = std::make_shared<QMetaObject::Connection>();
    *con_handle = connect(
        this,
        &Device::DeviceInterface::deviceChanged,
        this,
        [this, con_handle, data = std::move(score_device)](
            auto oldd, auto newd) {
          if (newd)
          {
            recreate(std::move(data));
            QObject::disconnect(*con_handle);
          }
        });
  }
  else
  {
    // We're already disconnected
    m_settings = newsettings;
  }
}

void DeviceInterface::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
  if (auto dev = getDevice())
  {
    if (auto node = findNodeFromPath(currentAddr.path, *dev))
    {
      bool is_listening = m_callbacks.find(currentAddr) != m_callbacks.end();

      if (!settings.value.valid())
      {
        if (is_listening)
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
        if (auto p = node->get_parameter())
          updateOSSIAAddress(settings, *p);
        else
          createOSSIAAddress(settings, *node);
      }

      auto newName = settings.address.path.last();
      if (!latin_compare(newName, node->get_name()))
      {
        renameListening_impl(currentAddr, newName);
        node->set_name(newName.toStdString());
      }
    }
    else
    {
      qDebug() << "updateAddress: Warning ! did not find" << currentAddr;
    }
  }
}

void DeviceInterface::removeListening_impl(
    ossia::net::node_base& node,
    State::Address addr)
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

void DeviceInterface::removeListening_impl(
    ossia::net::node_base& node,
    State::Address addr,
    std::vector<State::Address>& vec)
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
  if (child.path.size() < p_size)
    return false;
  for (int i = 0; i < p_size; i++)
  {
    if (child.path[i] != parent.path[i])
      return false;
  }
  return true;
}

void DeviceInterface::renameListening_impl(
    const State::Address& parent,
    const QString& newName)
{
  // Store the elements that are renamed
  std::vector<std::pair<State::Address, callback_pair>> saved_elts;
  for (auto it = m_callbacks.begin(); it != m_callbacks.end();)
  {
    if (is_parent(parent, it.key()))
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
    p.second.first->replace_callback(
        p.second.second, [this, addr = p.first](const ossia::value& val) {
          valueUpdated(addr, val);
        });
    m_callbacks.insert(std::move(p));
  }
}

namespace
{
struct in_sink final : public spdlog::sinks::sink
{
  const DeviceInterface& m_dev;
  in_sink(const DeviceInterface& dev)
      : m_dev{dev}
  {
  }
  void log(const spdlog::details::log_msg& msg) override
  {
    m_dev.logInbound(
        QString::fromLatin1(msg.payload.data(), msg.payload.size()));
  }

  void flush() override { }
  void set_pattern(const std::string& pattern) override { }
  void
  set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override
  {
  }
};
struct out_sink final : public spdlog::sinks::sink
{
  const DeviceInterface& m_dev;
  out_sink(const DeviceInterface& dev)
      : m_dev{dev}
  {
  }
  void log(const spdlog::details::log_msg& msg) override
  {
    m_dev.logOutbound(
        QString::fromLatin1(msg.payload.data(), msg.payload.size()));
  }

  void flush() override { }
  void set_pattern(const std::string& pattern) override { }
  void
  set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override
  {
  }
};
}
void DeviceInterface::setLogging_impl(DeviceLogging b) const
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
        logger.inbound_logger = std::make_shared<ossia::logger_type>(
            "in_logger", std::make_shared<in_sink>(*this));
        logger.outbound_logger = std::make_shared<ossia::logger_type>(
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
        logger.inbound_listened_logger = std::make_shared<ossia::logger_type>(
            "in_logger", std::make_shared<in_sink>(*this));
        logger.outbound_listened_logger = std::make_shared<ossia::logger_type>(
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

void DeviceInterface::enableCallbacks()
{
  if (!m_callbacksEnabled)
  {
    auto dev = getDevice();
    if (dev)
    {
      dev->on_node_created.connect<&DeviceInterface::nodeCreated>(this);
      dev->on_node_removing.connect<&DeviceInterface::nodeRemoving>(this);
      dev->on_node_renamed.connect<&DeviceInterface::nodeRenamed>(this);
      dev->on_parameter_created.connect<&DeviceInterface::addressCreated>(
          this);
      dev->on_parameter_removing.connect<&DeviceInterface::addressRemoved>(
          this);
      dev->on_attribute_modified.connect<&DeviceInterface::addressUpdated>(
          this);
    }
    m_callbacksEnabled = true;
  }
}

void DeviceInterface::disableCallbacks()
{
  if (m_callbacksEnabled)
  {
    auto dev = getDevice();
    if (dev)
    {
      dev->on_node_created.disconnect<&DeviceInterface::nodeCreated>(this);
      dev->on_node_removing.disconnect<&DeviceInterface::nodeRemoving>(this);
      dev->on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>(this);
      dev->on_parameter_created.disconnect<&DeviceInterface::addressCreated>(
          this);
      dev->on_parameter_removing.disconnect<&DeviceInterface::addressRemoved>(
          this);
      dev->on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>(
          this);
    }
    m_callbacksEnabled = false;
  }
}

Device::Node DeviceInterface::simple_refresh()
{
  Device::Node score_device{settings(), nullptr};

  // Recurse on the children
  const auto& ossia_children = getDevice()->get_root_node().children();
  score_device.reserve(ossia_children.size());
  for (const auto& node : ossia_children)
  {
    score_device.push_back(ToDeviceExplorer(*node.get()));
  }

  score_device.get<Device::DeviceSettings>().name
      = QString::fromStdString(getDevice()->get_name());

  return score_device;
}

void DeviceInterface::removeNode(const State::Address& address)
{
  using namespace ossia;
  if (!m_capas.canRemoveNode)
    return;
  if (auto dev = getDevice())
  {
    if (auto node = findNodeFromPath(address.path, *dev))
    {
      auto parent = node->get_parent();
      if (parent->has_child(*node))
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
    else
    {
      qDebug() << "Warning ! removeNode: did not find" << address;
    }
  }
}

Device::Node DeviceInterface::refresh()
{
  Device::Node device_node{settings(), nullptr};

  if (auto dev = getDevice())
  {
    auto& root = dev->get_root_node();
    // Clear the listening
    removeListening_impl(root, State::Address{m_settings.name, {}});

    disableCallbacks();
    m_callbacks.clear();

    auto fut = dev->get_protocol().update_async(root);
    auto res = fut.wait_for(std::chrono::seconds(1));
    if (res == std::future_status::ready)
    {
      // Make a device explorer node from the current state of the device.
      // First make the node corresponding to the root node.

      // Recurse on the children
      const auto& children = root.children();
      device_node.reserve(children.size());
      for (const auto& node : children)
      {
        device_node.push_back(ToDeviceExplorer(*node.get()));
      }
    }
    enableCallbacks();

    device_node.get<Device::DeviceSettings>().name = settings().name;
  }

  return device_node;
}

std::optional<ossia::value>
DeviceInterface::refresh(const State::Address& address)
{
  if (auto dev = getDevice())
  {
    auto node = findNodeFromPath(address.path, *dev);
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

void DeviceInterface::request(const Device::Node& address)
{
  if (auto dev = getDevice())
  {
    auto node = findNodeFromPath(address, *dev);
    if (node)
    {
      if (auto addr = node->get_parameter())
      {
        addr->request_value();
      }
    }
  }
}

Device::Node DeviceInterface::getNode(const State::Address& address) const
{
  if (auto dev = getDevice())
  {
    auto ossia_node = findNodeFromPath(address.path, *dev);
    if (ossia_node)
      return ToDeviceExplorer(*ossia_node);
  }

  return {};
}

Device::Node
DeviceInterface::getNodeWithoutChildren(const State::Address& address) const
{
  if (auto dev = getDevice())
  {
    auto ossia_node = findNodeFromPath(address.path, *dev);
    if (ossia_node)
    {
      return Device::Node{ToAddressSettings(*ossia_node), nullptr};
    }
  }

  return {};
}

void DeviceInterface::setListening(const State::Address& addr, bool b)
{
  if (auto dev = getDevice())
  {
    // First check if the address is already listening
    // so that we don't have to go through the tree.
    auto cb_it = m_callbacks.find(addr);

    ossia::net::parameter_base* ossia_addr{};
    if (cb_it == m_callbacks.end())
    {
      auto n = findNodeFromPath(addr.path, *dev);
      if (!n)
      {
        return;
      }

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
        m_callbacks.insert(
            {addr,
             {ossia_addr,
              ossia_addr->add_callback([=](const ossia::value& val) {
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

std::vector<State::Address> DeviceInterface::listening() const
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

void DeviceInterface::addToListening(
    const std::vector<State::Address>& addresses)
{
  if (!connected())
    return;

  for (const auto& addr : addresses)
  {
    SCORE_ASSERT(addr.device == this->settings().name);
    setListening(addr, true);
  }
}

void DeviceInterface::sendMessage(
    const State::Address& addr,
    const ossia::value& v)
{
  if (auto dev = getDevice())
  {
    if (auto node = findNodeFromPath(addr.path, *dev))
    {
      auto addr = node->get_parameter();
      if (addr)
      {
        addr->push_value(v);
      }
    }
    else
    {
      qDebug() << "Warning! sendMessage: did not find" << addr;
    }
  }
}

bool DeviceInterface::isLogging() const
{
  return m_logging;
}

void DeviceInterface::setLogging(DeviceLogging b)
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

OwningDeviceInterface::~OwningDeviceInterface() { }

void OwningDeviceInterface::replaceDevice(ossia::net::device_base* d)
{
  disconnect();
  m_dev.reset(d);
  deviceChanged(nullptr, d);
  m_owned = false;
}

void OwningDeviceInterface::releaseDevice()
{
  DeviceInterface::disconnect();
  deviceChanged(m_dev.get(), nullptr);
  m_dev.release();
}

void OwningDeviceInterface::disconnect()
{
  if (m_owned)
  {
    DeviceInterface::disconnect();
    // TODO why not auto dev = m_dev; ... like in MIDIDevice ?
    deviceChanged(m_dev.get(), nullptr);
    m_dev.reset();
  }
}

void DeviceInterface::nodeCreated(const ossia::net::node_base& n)
{
  pathAdded(ToAddress(n));
}

void DeviceInterface::nodeRemoving(const ossia::net::node_base& n)
{
  pathRemoved(ToAddress(n));
}

void DeviceInterface::nodeRenamed(
    const ossia::net::node_base& node,
    std::string old_name)
{
  if (!node.get_parent())
    return;

  State::Address currentAddress = ToAddress(*node.get_parent());
  currentAddress.path.push_back(QString::fromStdString(old_name));

  Device::AddressSettings as = ToAddressSettings(node);
  as.name = QString::fromStdString(node.get_name());

  renameListening_impl(currentAddress, as.name);

  pathUpdated(currentAddress, as);
}

void DeviceInterface::addressCreated(const ossia::net::parameter_base& addr)
{
  State::Address currentAddress = ToAddress(addr.get_node());
  Device::AddressSettings as = ToAddressSettings(addr.get_node());
  pathUpdated(currentAddress, as);
  setListening(currentAddress, true);
}

void DeviceInterface::addressUpdated(
    const ossia::net::node_base& node,
    ossia::string_view key)
{
  const bool hidden
      = (ossia::net::get_zombie(node) || ossia::net::get_hidden(node));

  State::Address currentAddress = ToAddress(node);
  if (hidden)
  {
    pathRemoved(currentAddress);
    return;
  }

  Device::AddressSettings as = ToAddressSettings(node);

  pathUpdated(currentAddress, as);
}

void DeviceInterface::addressRemoved(const ossia::net::parameter_base& addr)
{
  auto address = ToAddress(addr.get_node());

  auto cb_it = m_callbacks.find(address);
  if (cb_it != m_callbacks.end())
  {
    m_callbacks.erase(cb_it);
  }
  auto& node = addr.get_node();
  State::Address currentAddress = ToAddress(node);
  Device::AddressSettings as;
  as.name = QString::fromStdString(node.get_name());
  pathUpdated(currentAddress, as);
}

void releaseDevice(ossia::net::network_context& ctx, std::unique_ptr<ossia::net::device_base> dd)
{
  if(dd)
  {
    std::atomic_int k{};
    boost::asio::post(
          ctx.context,
          [&dev=dd, ctx = &ctx.context, &k] () mutable
    {
      dev->get_protocol().stop();
      boost::asio::post(*ctx, [&k] {
        k = 1;
      });
    });

    int n = 0;
    while(k != 1 && n < 1000)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ++n;
    }

    boost::asio::post(ctx.context, [d = std::move(dd)] () mutable {
      d.reset();
    });
  }
}

}
