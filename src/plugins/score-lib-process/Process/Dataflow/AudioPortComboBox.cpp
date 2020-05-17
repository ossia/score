#include "AudioPortComboBox.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::AudioPortComboBox)

namespace Process
{
AudioPortComboBox::AudioPortComboBox(
    const State::Address& rootAddress,
    const Device::Node& node,
    QWidget* parent)
    : QComboBox{parent}, m_root{rootAddress}
{
  m_address.address = rootAddress;
  if (!node.children().empty())
  {
    auto& first_child = node.children().front().target<Device::AddressSettings>()->name;
    m_address.address.path.push_back(first_child);
  }

  connect(
      this, qOverload<const QString&>(&QComboBox::currentIndexChanged), [=](const QString& str) {
        if (str == "None")
        {
          if (m_address.address != m_root)
          {
            m_address.address = m_root;
            addressChanged(m_address);
          }
        }
        else
        {
          auto addr = m_root;
          addr.path.push_back(str);
          if (m_address.address != addr)
          {
            m_address.address = std::move(addr);
            addressChanged(m_address);
          }
        }
      });

  m_child.push_back("None");
  addItem("None");
  for (auto& child : node)
  {
    const QString& name = child.target<Device::AddressSettings>()->name;
    addItem(name);
    m_child.push_back(name);
  }
}

void AudioPortComboBox::setAddress(const State::Address& addr)
{
  m_address.address = m_root;

  if (!addr.path.empty())
  {
    auto& name = addr.path.back();
    for (int i = 0; i < m_child.size(); i++)
    {
      if (m_child[i] == name)
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

QWidget* makeAddressCombo(
    State::Address root,
    const Device::Node& out_node,
    const Process::Port& port,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  using namespace Device;
  auto edit = new Process::AudioPortComboBox{root, out_node, parent};
  edit->setAddress(port.address().address);

  QObject::connect(
      &port, &Process::Port::addressChanged, edit, [edit](const State::AddressAccessor& addr) {
        if (addr.address != edit->address().address)
        {
          edit->setAddress(addr.address);
        }
      });

  QObject::connect(
      edit,
      &Process::AudioPortComboBox::addressChanged,
      parent,
      [&port, &ctx](const auto& newAddr) {
        if (newAddr.address == port.address().address)
          return;

        CommandDispatcher<>{ctx.dispatcher}.submit(
            new Process::ChangePortAddress{port, State::AddressAccessor{newAddr.address, {}}});
      });

  return edit;
}

QWidget* makeMidiCombo(
    QStringList devices,
    const Process::Port& port,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  using namespace Device;
  auto edit = new QComboBox{parent};
  edit->addItems(devices);

  edit->setCurrentText(port.address().address.device);

  QObject::connect(
      &port, &Process::Port::addressChanged, edit, [edit](const State::AddressAccessor& addr) {
        if (addr.address.device != edit->currentText())
        {
          edit->setCurrentText(addr.address.device);
        }
      });

  QObject::connect(
      edit, &QComboBox::currentTextChanged, parent, [&port, &ctx](const auto& newDev) {
        if (newDev == port.address().address.device)
          return;

        CommandDispatcher<>{ctx.dispatcher}.submit(new Process::ChangePortAddress{
            port, State::AddressAccessor{State::Address{newDev, {}}, {}}});
      });

  return edit;
}
}
