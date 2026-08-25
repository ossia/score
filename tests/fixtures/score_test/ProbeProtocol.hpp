#pragma once

// A protocol that exists but builds nothing, for tests about whether score
// *asks* for a device rather than what it gets back.
//
// Returning null is a case the explorer already handles -- it is what a
// protocol whose hardware is absent does -- so the request count is what is
// under test. A protocol the build genuinely has is the point: not building a
// device then reads as a decision rather than an absence.

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>

namespace score::test
{

//! A device that is really there, whose state a test can drive.
//!
//! ProbeProtocolFactory deliberately builds none -- it is for tests about
//! whether score *asks*. But a host with no DeviceInterface at all reports
//! nothing about its devices, so everything that carries device state to a peer
//! can be deleted without a single test noticing. This protocol owns one.
struct ProbeDevice final : public Device::DeviceInterface
{
  explicit ProbeDevice(Device::DeviceSettings s)
      : Device::DeviceInterface{std::move(s)}
  {
    m_capas.nodeKinds = Device::NodeKind::MidiIn | Device::NodeKind::TextureOut;
  }

  bool reconnect() override { return m_connected; }
  ossia::net::device_base* getDevice() const override { return nullptr; }
  bool connected() const override { return m_connected; }

  void setConnected(bool b)
  {
    m_connected = b;
    connectionChanged(b);
  }

  bool m_connected{true};
};

struct ConnectedProbeProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("2b7d6d05-4f1a-4c1e-9a2b-5e0f0d5b7c31")
public:
  static inline ProbeDevice* last = nullptr;

  QString prettyName() const noexcept override
  {
    return QStringLiteral("Connected probe");
  }
  QString category() const noexcept override { return StandardCategories::util; }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& s, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    last = new ProbeDevice{s};
    return last;
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings d;
      d.protocol = static_concreteKey();
      d.name = QStringLiteral("ConnectedProbe");
      return d;
    }();
    return s;
  }

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface&, const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface&,
      const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override { return nullptr; }

  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  void serializeProtocolSpecificSettings(const QVariant&, const VisitorVariant&)
      const override
  {
  }
  bool checkCompatibility(
      const Device::DeviceSettings&, const Device::DeviceSettings&) const noexcept override
  {
    return true;
  }
};

//! Two devices under one heading, so that a test can tell a category from a
//! name without depending on what is plugged into the machine.
struct ProbeEnumerator final : public Device::DeviceEnumerator
{
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    for(const auto& name : {QStringLiteral("probe-one"), QStringLiteral("probe-two")})
    {
      Device::DeviceSettings s;
      s.name = name;
      f(name, s);
    }
  }
};

struct ProbeProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("9f1c0f4a-1b2c-4d3e-8f70-0badc0ffee00")
public:
  static inline int requests = 0;

  QString prettyName() const noexcept override { return QStringLiteral("Probe"); }
  QString category() const noexcept override { return StandardCategories::util; }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings&, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    ++requests;
    return nullptr;
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings d;
      d.protocol = static_concreteKey();
      d.name = QStringLiteral("Probe");
      return d;
    }();
    return s;
  }

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface&, const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface&,
      const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override { return nullptr; }

  static inline const QString enumeratorCategory{QStringLiteral("Probes")};
  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext&) const override
  {
    return {{enumeratorCategory, new ProbeEnumerator}};
  }

  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  void serializeProtocolSpecificSettings(const QVariant&, const VisitorVariant&)
      const override
  {
  }
  bool checkCompatibility(
      const Device::DeviceSettings&, const Device::DeviceSettings&) const noexcept override
  {
    return true;
  }
};

//! Registered into the running application, so that the protocol is one this
//! build genuinely has.
inline void register_probe_protocol(const score::GUIApplicationContext& ctx)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  if(list.get(ProbeProtocolFactory::static_concreteKey()))
    return;
  const_cast<Device::ProtocolFactoryList&>(list).insert(
      std::make_unique<ProbeProtocolFactory>());
}

inline void register_connected_probe_protocol(const score::GUIApplicationContext& ctx)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  if(list.get(ConnectedProbeProtocolFactory::static_concreteKey()))
    return;
  const_cast<Device::ProtocolFactoryList&>(list).insert(
      std::make_unique<ConnectedProbeProtocolFactory>());
}

inline Device::Node connected_probe_device_node(const QString& name)
{
  Device::DeviceSettings s;
  s.protocol = ConnectedProbeProtocolFactory::static_concreteKey();
  s.name = name;
  return Device::Node{s, nullptr};
}

inline Device::Node probe_device_node(const QString& name)
{
  Device::DeviceSettings s;
  s.protocol = ProbeProtocolFactory::static_concreteKey();
  s.name = name;
  return Device::Node{s, nullptr};
}

}
