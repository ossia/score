#include "PortAddressComboBox.hpp"

#include <State/MessageListSerialization.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::PortAddressComboBox)

namespace Process
{
Device::NodeKind nodeKindOf(PortType type, bool inlet) noexcept
{
  using K = Device::NodeKind;
  switch(type)
  {
    case PortType::Message:
      return K::Value;
    case PortType::Audio:
      return inlet ? K::AudioIn : K::AudioOut;
    case PortType::Midi:
      return inlet ? K::MidiIn : K::MidiOut;
    case PortType::Texture:
      return inlet ? K::TextureIn : K::TextureOut;
    case PortType::Geometry:
      return inlet ? K::GeometryIn : K::GeometryOut;
  }
  return K::None;
}

namespace
{
// Three cases for an audio node: a pure input (usable by inlets), a pure
// output (usable by outlets), or a virtual port which goes both ways.
bool audioMatches(
    const ossia::net::node_base& n, const ossia::net::parameter_base& p,
    bool inlet) noexcept
{
  if(dynamic_cast<const ossia::virtual_audio_parameter*>(&p))
    return true;

  // The custom ports added to the audio device know their direction...
  if(auto m = dynamic_cast<const ossia::mapped_audio_parameter*>(&p))
    return inlet ? !m->is_output : m->is_output;

  // ... the hardware ports libossia sets up (/in/N, /in/main, /out/N...) only
  // by where they sit.
  for(auto node = &n; node; node = node->get_parent())
  {
    if(auto parent = node->get_parent(); parent && !parent->get_parent())
    {
      const auto& top = node->get_name();
      if(top == "in")
        return inlet;
      if(top == "out")
        return !inlet;
      break;
    }
  }

  // The streams of other devices (libav, gstreamer...) follow the device's
  // declared kinds.
  return true;
}

bool nodeMatches(const ossia::net::node_base& n, PortType type, bool inlet) noexcept
{
  auto p = n.get_parameter();
  if(!p)
    return false;

  switch(type)
  {
    case PortType::Audio:
      return p->get_type() == ossia::parameter_type::AUDIO && audioMatches(n, *p, inlet);
    case PortType::Texture:
      return p->get_type() == ossia::parameter_type::TEXTURE;
    case PortType::Geometry:
      return p->get_type() == ossia::parameter_type::GEOMETRY;
    default:
      return false;
  }
}

void walk(
    const ossia::net::node_base& n, State::Address& addr, PortType type, bool inlet,
    std::vector<State::Address>& out)
{
  if(nodeMatches(n, type, inlet))
    out.push_back(addr);

  for(auto child : n.children_copy())
  {
    addr.path.push_back(QString::fromStdString(child->get_name()));
    walk(*child, addr, type, inlet, out);
    addr.path.pop_back();
  }
}
}

std::vector<State::Address>
listPortAddresses(Device::DeviceInterface& dev, PortType type, bool inlet)
{
  std::vector<State::Address> out;
  if(!Device::carries(dev.capabilities().nodeKinds, nodeKindOf(type, inlet)))
    return out;

  auto ossia_dev = dev.getDevice();
  if(!ossia_dev)
    return out;

  auto& root = ossia_dev->get_root_node();
  State::Address addr{dev.name(), {}};

  if(type == PortType::Midi)
  {
    out.push_back(addr);
    for(auto child : root.children_copy())
    {
      addr.path = {QString::fromStdString(child->get_name())};
      out.push_back(addr);
    }
    return out;
  }

  walk(root, addr, type, inlet, out);
  return out;
}

std::vector<State::Address>
listPortAddresses(Device::DeviceList& devices, PortType type, bool inlet)
{
  std::vector<State::Address> out;
  devices.apply([&](Device::DeviceInterface& dev) {
    auto addrs = listPortAddresses(dev, type, inlet);
    out.insert(out.end(), addrs.begin(), addrs.end());
  });
  return out;
}

PortAddressComboBox::PortAddressComboBox(
    Device::DeviceList& devices, PortType type, bool inlet, QWidget* parent)
    : QComboBox{parent}
    , m_devices{devices}
    , m_type{type}
    , m_inlet{inlet}
{
  setEditable(true);
  setInsertPolicy(QComboBox::NoInsert);
  setAcceptDrops(true);
  // Addresses can be long: never let the widest one dictate the panel's width
  setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  setMinimumContentsLength(8);
  lineEdit()->setPlaceholderText(tr("None"));

  connect(this, qOverload<int>(&QComboBox::activated), this, [this](int) {
    commitText();
  });
  connect(lineEdit(), &QLineEdit::editingFinished, this, &PortAddressComboBox::commitText);

  devices.apply([this](Device::DeviceInterface& dev) { watch(dev); });
  connect(
      &devices, &Device::DeviceList::deviceAdded, this,
      [this](Device::DeviceInterface* dev) {
    watch(*dev);
    scheduleReload();
      });
  connect(
      &devices, &Device::DeviceList::deviceRemoved, this,
      [this](Device::DeviceInterface*) { scheduleReload(); });

  reload();
}

PortAddressComboBox::~PortAddressComboBox() { }

void PortAddressComboBox::watch(Device::DeviceInterface& dev)
{
  // A device may start carrying our kind when it reconnects (libav flips
  // between input and output), so the coarse events are followed everywhere;
  // the per-node ones only on devices already worth walking.
  auto reload = [this] { scheduleReload(); };
  connect(&dev, &Device::DeviceInterface::deviceChanged, this, reload);
  connect(&dev, &Device::DeviceInterface::namespaceUpdated, this, reload);
  connect(&dev, &Device::DeviceInterface::connectionChanged, this, reload);

  if(Device::carries(dev.capabilities().nodeKinds, nodeKindOf(m_type, m_inlet)))
  {
    connect(&dev, &Device::DeviceInterface::pathAdded, this, reload);
    connect(&dev, &Device::DeviceInterface::pathRemoved, this, reload);
    connect(&dev, &Device::DeviceInterface::pathUpdated, this, reload);
  }
}

void PortAddressComboBox::scheduleReload()
{
  if(m_reloadPending)
    return;
  m_reloadPending = true;
  QTimer::singleShot(0, this, [this] {
    m_reloadPending = false;
    reload();
  });
}

void PortAddressComboBox::reload()
{
  const QSignalBlocker block{this};
  clear();
  addItem(QString{});
  for(const auto& addr : listPortAddresses(m_devices, m_type, m_inlet))
    addItem(addr.toString());

  // What the machine running the score reported. Only the device is known
  // here -- its tree lives where the device does -- so it is offered as the
  // device address, which is what an audio / MIDI / texture port binds to.
  if(remoteDevices)
  {
    for(const auto& dev : remoteDevices())
    {
      const auto txt = State::Address{dev, {}}.toString();
      if(findText(txt) < 0)
        addItem(txt);
    }
  }
  showAddress();
}

void PortAddressComboBox::showAddress()
{
  const QSignalBlocker block{this};
  const auto txt
      = m_address.address.device.isEmpty() ? QString{} : m_address.toString_unsafe();
  if(int idx = findText(txt); idx >= 0)
    setCurrentIndex(idx);
  else
    setCurrentIndex(0);
  // Still shown when it is not in the list (device down, typed by hand...)
  setEditText(txt);
}

void PortAddressComboBox::setAddress(const State::AddressAccessor& addr)
{
  m_address = addr;
  showAddress();
}

void PortAddressComboBox::commitText()
{
  const auto txt = currentText().trimmed();
  if(txt.isEmpty())
  {
    commit({});
    return;
  }

  if(auto addr = State::parseAddressAccessor(txt))
    commit(std::move(*addr));
  else
    showAddress(); // not an address: back to what the port has
}

void PortAddressComboBox::commit(State::AddressAccessor addr)
{
  if(addr == m_address)
  {
    showAddress();
    return;
  }
  m_address = std::move(addr);
  showAddress();
  addressChanged(m_address);
}

void PortAddressComboBox::dragEnterEvent(QDragEnterEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if(formats.contains(score::mime::messagelist())
     || formats.contains(score::mime::nodelist()))
    event->acceptProposedAction();
}

void PortAddressComboBox::dropEvent(QDropEvent* event)
{
  auto& mime = *event->mimeData();
  if(mime.hasFormat(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    if(auto res = Device::addressOfDroppedNodes(des.deserialize(), m_address))
      commit(std::move(res->address));
    event->acceptProposedAction();
  }
  else if(mime.hasFormat(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    auto ml = des.deserialize();
    if(!ml.empty())
      commit(ml.front().address);
    event->acceptProposedAction();
  }
}

QComboBox* makePortAddressCombo(
    const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent)
{
  auto& devices = ctx.plugin<Explorer::DeviceDocumentPlugin>().list();
  const bool inlet = qobject_cast<const Process::Inlet*>(&port) != nullptr;

  auto edit = new PortAddressComboBox{devices, port.type(), inlet, parent};
  edit->setAddress(port.address());

  if(auto* plug = ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    const auto kind = nodeKindOf(port.type(), inlet);
    edit->remoteDevices = [plug, kind] { return plug->remoteDevicesOfKind(kind); };
    // The peer answers after the join, so anything already showing a list has
    // to be told rather than asked once.
    QObject::connect(
        plug, &Explorer::DeviceDocumentPlugin::remoteKindsChanged, edit,
        [edit](const QString&) { edit->reload(); });
  }

  QObject::connect(
      &port, &Process::Port::addressChanged, edit,
      [edit](const State::AddressAccessor& addr) {
    if(addr != edit->address())
      edit->setAddress(addr);
      });

  QObject::connect(
      edit, &PortAddressComboBox::addressChanged, parent,
      [&port, &ctx](const State::AddressAccessor& addr) {
    if(addr == port.address())
      return;

    CommandDispatcher<>{ctx.dispatcher}.submit(
        new Process::ChangePortAddress{port, addr});
      });

  return edit;
}
}
