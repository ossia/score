#include "HIDProtocolFactory.hpp"

#include "HIDDevice.hpp"
#include "HIDProtocolSettingsWidget.hpp"
#include "HIDSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/protocols/hid/hid_device.hpp>

#include <QObject>
#include <QTimer>
#include <QUrl>

namespace Protocols
{
Device::DeviceSettings hidDeviceSettings(const ossia::net::hid_device_info& info)
{
  Device::DeviceSettings s;
  s.name = QString::fromStdString(info.product.empty() ? "HID" : info.product);
  s.protocol = HIDProtocolFactory::static_concreteKey();

  HIDSpecificSettings specif;
  specif.path = QString::fromStdString(info.path);
  specif.serial = QString::fromStdString(info.serial);
  specif.product = QString::fromStdString(info.product);
  specif.vendorId = info.vendor_id;
  specif.productId = info.product_id;
  specif.interfaceNumber = info.interface_number;
  s.deviceSpecificSettings = QVariant::fromValue(specif);
  return s;
}

namespace
{
class HIDEnumerator final : public Device::DeviceEnumerator
{
public:
  HIDEnumerator()
  {
    m_timer.setInterval(1000);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { rescan(); });
    m_timer.start();
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    m_current = scan();
    for(auto& [name, set] : m_current)
      f(name, set);
  }

private:
  using device_list = std::vector<std::pair<QString, Device::DeviceSettings>>;

  static device_list scan()
  {
    device_list res;
    for(const auto& d : ossia::net::hid_enumerate())
      res.emplace_back(QString::fromStdString(d.display_name()), hidDeviceSettings(d));
    return res;
  }

  void rescan()
  {
    if(const auto count = ossia::net::hid_device_change_count(); count == m_changeCount)
      return;
    else
      m_changeCount = count;

    auto next = scan();
    auto has = [](const device_list& l, const QString& n) {
      return ossia::any_of(l, [&n](const auto& p) { return p.first == n; });
    };

    for(auto& [name, set] : m_current)
      if(!has(next, name))
        deviceRemoved(name);

    bool added = false;
    for(auto& [name, set] : next)
    {
      if(!has(m_current, name))
      {
        deviceAdded(name, set);
        added = true;
      }
    }

    m_current = std::move(next);
    if(added)
      sort();
  }

  QTimer m_timer;
  uint32_t m_changeCount{};
  mutable device_list m_current;
};
}

QString HIDProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("HID");
}

QString HIDProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl HIDProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/hid-device.html");
}

Device::DeviceEnumerators
HIDProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Devices", new HIDEnumerator}};
}

Device::DeviceInterface* HIDProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new HIDDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& HIDProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "HID";
    s.deviceSpecificSettings = QVariant::fromValue(HIDSpecificSettings{});
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* HIDProtocolFactory::makeSettingsWidget()
{
  return new HIDProtocolSettingsWidget;
}

QVariant
HIDProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<HIDSpecificSettings>(visitor);
}

void HIDProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<HIDSpecificSettings>(data, visitor);
}

bool HIDProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  if(a.protocol != b.protocol)
    return true;
  auto a_ = a.deviceSpecificSettings.value<HIDSpecificSettings>();
  auto b_ = b.deviceSpecificSettings.value<HIDSpecificSettings>();
  return a_.path != b_.path;
}
}
