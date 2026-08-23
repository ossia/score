#pragma once
// A fake protocol for the device explorer tests: its "remote namespace" is the
// list of names stored in its device-specific settings, so that editing the
// settings is editing what the device will find when it reconnects. Knobs in
// FakeOptions (g_opts) make it connect synchronously, from the event loop, or
// not at all. One registration per process: see registerFakeProtocol().

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/Interface.hpp>

#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace Explorer
{
class DeviceDocumentPlugin;
}

namespace score::test::fake
{
struct FakeOptions
{
  bool canRefreshTree{true};
  bool canSerialize{true};
  //! The device connects (false: like an OSCQuery host that is down).
  bool available{true};
  //! refresh() rebuilds the tree from the remote, dropping what was replayed into
  //! the device (an OSCQuery mirror); otherwise it snapshots what the device has
  //! (audio, MIDI...).
  bool remoteAuthoritative{false};
  //! The device connects from the event loop rather than right away, like the
  //! protocols which resolve their host in a thread.
  bool deferConnect{false};

  int refreshCount{};
  int connectCount{};
};
inline FakeOptions g_opts;

inline QStringList remoteOf(const Device::DeviceSettings& s)
{
  return s.deviceSpecificSettings.toStringList();
}

class FakeDevice final : public Device::OwningDeviceInterface
{
public:
  explicit FakeDevice(const Device::DeviceSettings& s)
      : OwningDeviceInterface{s}
  {
    m_capas.canRefreshTree = g_opts.canRefreshTree;
    m_capas.canSerialize = g_opts.canSerialize;
    m_capas.canLearn = true;
  }

  bool reconnect() override
  {
    disconnect();
    if(!g_opts.available)
    {
      connectionChanged(false);
      return false;
    }

    if(g_opts.deferConnect)
    {
      QTimer::singleShot(0, this, [self = QPointer{this}] {
        if(self)
          self->connectNow();
      });
      return false;
    }

    connectNow();
    return true;
  }

  void connectNow()
  {
    g_opts.connectCount++;
    auto dev = std::make_shared<ossia::net::generic_device>(
        std::make_unique<ossia::net::multiplex_protocol>(),
        settings().name.toStdString());
    for(const auto& name : remoteOf(settings()))
      ossia::create_parameter(dev->get_root_node(), name.toStdString(), "float");

    // As the real protocols do: the previous device was torn down by
    // disconnect() (callbacks cleared, deviceChanged(old, nullptr)); install
    // the new one and announce it. Not replaceDevice(), which is for devices
    // the interface does not own.
    auto old = m_dev;
    m_dev = dev;
    deviceChanged(old.get(), m_dev.get());
    connectionChanged(true);
  }

  void recreate(const Device::Node& n) override
  {
    for(auto& child : n)
      addNode(child);
  }

  Device::Node refresh() override
  {
    g_opts.refreshCount++;
    if(!connected())
      return Device::Node{settings(), nullptr};

    // An empty remote stands for a host that does not answer in time: the
    // exploration yields nothing and the device's nodes are left alone.
    const auto remote = remoteOf(settings());
    if(remote.isEmpty())
      return Device::Node{settings(), nullptr};

    if(g_opts.remoteAuthoritative)
    {
      // Like a remote-authoritative protocol (the OSCQuery mirror clears its
      // tree when the namespace arrives), and like DeviceInterface::refresh(),
      // which drops the value callbacks before the nodes are rebuilt.
      auto& root = m_dev->get_root_node();
      removeListening_impl(root, State::Address{settings().name, {}});
      root.clear_children();
      for(const auto& name : remote)
        ossia::create_parameter(root, name.toStdString(), "float");
    }
    // Else: a device that builds its own tree (audio, MIDI...) just reports it
    return simple_refresh();
  }
};

class FakeSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Device::DeviceSettings m_settings;
  Device::DeviceSettings getSettings() const override { return m_settings; }
  void setSettings(const Device::DeviceSettings& s) override { m_settings = s; }
};

class FakeFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("0b1f4c6e-2d3a-4e5f-8a9b-7c6d5e4f3a2b")
public:
  QString prettyName() const noexcept override { return QStringLiteral("Fake"); }
  QString category() const noexcept override { return QStringLiteral("Test"); }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& s, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    return new FakeDevice{s};
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new FakeSettingsWidget;
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
  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings set;
      set.name = QStringLiteral("fake");
      set.protocol = static_concreteKey();
      return set;
    }();
    return s;
  }
  void serializeProtocolSpecificSettings(
      const QVariant&, const VisitorVariant&) const override
  {
  }
  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  bool checkCompatibility(const Device::DeviceSettings&, const Device::DeviceSettings&)
      const noexcept override
  {
    return true;
  }
};

inline Device::DeviceSettings fakeSettings(QString name, QStringList remote)
{
  Device::DeviceSettings s;
  s.name = std::move(name);
  s.protocol = FakeFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(std::move(remote));
  return s;
}

inline Device::Node leaf(const QString& name)
{
  Device::AddressSettings as;
  as.name = name;
  as.value = ossia::value{0};
  as.ioType = ossia::access_mode::BI;
  return Device::Node{as, nullptr};
}

//! Registers the fake protocol in the application, once per process.
inline void registerFakeProtocol(const score::GUIApplicationContext& ctx)
{
  auto& list = const_cast<Device::ProtocolFactoryList&>(
      ctx.interfaces<Device::ProtocolFactoryList>());
  if(!list.get(FakeFactory::static_concreteKey()))
    list.insert(std::make_unique<FakeFactory>());
}
}
