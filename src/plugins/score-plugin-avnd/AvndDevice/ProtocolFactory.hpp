#pragma once
#include <Explorer/DefaultProtocolFactory.hpp>
#include <Crousti/Concepts.hpp>

#include <State/Widgets/AddressFragmentLineEdit.hpp>
//#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/config.hpp>

#include "Device.hpp"

namespace oscr
{
template <typename Node>
class ProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE(avnd::get_uuid<Node>())

  QString prettyName() const noexcept override
  {
    return QObject::tr(avnd::get_name<Node>());
  }

  QString category() const noexcept override
  {
    return avnd::get_category<Node>();
  }

  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override
  {
    return nullptr;
  }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override
  {
    return new DeviceImplementation{settings};
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings& settings = [&]
    {
      Device::DeviceSettings s;
      s.protocol = concreteKey();
      s.name = avnd::get_name<Node>;
      return s;
    }();

    return settings;
  }

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant
  makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data,
      const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};
}
