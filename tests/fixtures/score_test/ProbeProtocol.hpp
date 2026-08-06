#pragma once

// A protocol that exists but builds nothing, for tests about whether score
// *asks* for a device rather than what it gets back.
//
// Returning null is a case the explorer already handles -- it is what a
// protocol whose hardware is absent does -- so the request count is what is
// under test. A protocol the build genuinely has is the point: not building a
// device then reads as a decision rather than an absence.

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>

namespace score::test
{

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

inline Device::Node probe_device_node(const QString& name)
{
  Device::DeviceSettings s;
  s.protocol = ProbeProtocolFactory::static_concreteKey();
  s.name = name;
  return Device::Node{s, nullptr};
}

}
