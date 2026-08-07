#include "AudioPortComboBox.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::AudioPortComboBox)

namespace Process
{
AudioPortComboBox::AudioPortComboBox(
    const State::Address& rootAddress, const Device::Node& node, QWidget* parent)
    : QComboBox{parent}
    , m_root{rootAddress}
{
  m_address.address = rootAddress;
  if(!node.children().empty())
  {
    auto& first_child = node.children().front().target<Device::AddressSettings>()->name;
    m_address.address.path.push_back(first_child);
  }

  connect(this, &QComboBox::currentTextChanged, [this](const QString& str) {
    if(str == "None")
    {
      if(m_address.address != m_root)
      {
        m_address.address = m_root;
        addressChanged(m_address);
      }
    }
    else
    {
      auto addr = m_root;
      addr.path.push_back(str);
      if(m_address.address != addr)
      {
        m_address.address = std::move(addr);
        addressChanged(m_address);
      }
    }
  });

  m_child.push_back("None");
  addItem("None");
  for(auto& child : node)
  {
    const QString& name = child.target<Device::AddressSettings>()->name;
    addItem(name);
    m_child.push_back(name);
  }
}

void AudioPortComboBox::setAddress(const State::Address& addr)
{
  m_address.address = m_root;

  if(!addr.path.empty())
  {
    auto& name = addr.path.back();
    for(std::size_t i = 0; i < m_child.size(); i++)
    {
      if(m_child[i] == name)
      {
        m_address.address.path.push_back(name);
        setCurrentIndex(i);
        return;
      }
    }
  }

  setCurrentIndex(0);
}

const Device::FullAddressSettings& AudioPortComboBox::address() const
{
  return m_address;
}

QComboBox* makeAddressCombo(
    State::Address root, const Device::Node& out_node, const Process::Port& port,
    const score::DocumentContext& ctx, QWidget* parent)
{
  using namespace Device;
  auto edit = new Process::AudioPortComboBox{root, out_node, parent};
  edit->setAddress(port.address().address);

  QObject::connect(
      &port, &Process::Port::addressChanged, edit,
      [edit](const State::AddressAccessor& addr) {
    if(addr.address != edit->address().address)
    {
      edit->setAddress(addr.address);
    }
      });

  QObject::connect(
      edit, &Process::AudioPortComboBox::addressChanged, parent,
      [&port, &ctx](const auto& newAddr) {
    if(newAddr.address == port.address().address)
      return;

    CommandDispatcher<>{ctx.dispatcher}.submit(new Process::ChangePortAddress{
        port, State::AddressAccessor{newAddr.address, {}}});
      });

  return edit;
}

std::optional<State::Address> droppedDeviceAddress(
    const Device::FreeNodeList& nodes, Process::PortType type) noexcept
{
  if(nodes.empty() || type == Process::PortType::Message)
    return std::nullopt;

  const auto& [address, node] = nodes.front();
  if(!node.template is<Device::DeviceSettings>() || address.device.isEmpty())
    return std::nullopt;

  return State::Address{address.device, {}};
}

QComboBox* makeDeviceCombo(
    Device::DeviceKind kind, Device::DeviceList& list, const Process::Port& port,
    const score::DocumentContext& ctx, QWidget* parent)
{
  using namespace Device;
  auto edit = new QComboBox{parent};
  edit->addItem("");

  auto on_add = [kind, edit](Device::DeviceInterface* dev) {
    if(dev && dev->kinds().testFlag(kind))
      edit->addItem(dev->settings().name);
  };
  list.apply([on_add](Device::DeviceInterface& dev) { on_add(&dev); });
  QObject::connect(&list, &Device::DeviceList::deviceAdded, edit, on_add);
  QObject::connect(
      &list, &Device::DeviceList::deviceRemoved, edit,
      [edit](Device::DeviceInterface* dev) {
    int idx = edit->findText(dev->settings().name);
    if(idx >= 0)
      edit->removeItem(idx);
  });

  // Nothing here is a device object when the score runs on another machine, so
  // the names come from what that machine reported instead.
  if(auto* plug = ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    auto on_remote = [kind, edit, plug] {
      for(const auto& name : plug->remoteDevicesOfKind(kind))
        if(edit->findText(name) < 0)
          edit->addItem(name);
    };
    on_remote();
    QObject::connect(
        plug, &Explorer::DeviceDocumentPlugin::remoteKindsChanged, edit, on_remote);
  }

  edit->setCurrentText(port.address().address.device);

  QObject::connect(
      &port, &Process::Port::addressChanged, edit,
      [edit](const State::AddressAccessor& addr) {
    if(addr.address.device != edit->currentText())
    {
      edit->setCurrentText(addr.address.device);
    }
      });

  QObject::connect(
      edit, &QComboBox::currentTextChanged, parent, [&port, &ctx](const auto& newDev) {
        if(newDev == port.address().address.device)
          return;

        CommandDispatcher<>{ctx.dispatcher}.submit(new Process::ChangePortAddress{
            port, State::AddressAccessor{State::Address{newDev, {}}, {}}});
      });

  return edit;
}
}
