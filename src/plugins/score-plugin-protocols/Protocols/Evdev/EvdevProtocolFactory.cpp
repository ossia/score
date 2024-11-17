#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include "EvdevDevice.hpp"
#include "EvdevProtocolFactory.hpp"
#include "EvdevProtocolSettingsWidget.hpp"
#include "EvdevSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QObject>
#include <QRegularExpression>

namespace Protocols
{

class EvdevEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    QFile devs("/proc/bus/input/devices");
    if(!devs.open(QIODevice::ReadOnly))
    {
      qDebug("Error while reading available devices");
      return;
    }
    for(const auto& dev : QString(devs.readAll()).split("\n\n"))
    {
      // clang-format off
      static const QRegularExpression N_rx{"N: Name=\"(.*)\""};
      static const QRegularExpression H_rx{"H: Handlers=.*(event[0-9]+)"};
      static const QRegularExpression I_rx{"I: Bus=([a-fA-F0-9]+) Vendor=([a-fA-F0-9]+) Product=([a-fA-F0-9]+) Version=([a-fA-F0-9]+)"};
      // clang-format on
      EvdevSpecificSettings specif;
      for(const auto& line : dev.split("\n"))
      {
        if(line.startsWith("N:"))
        {
          if(auto res = N_rx.match(line); res.hasMatch())
          {
            specif.name = res.captured(1);
          }
        }
        else if(line.startsWith("H:"))
        {
          if(auto res = H_rx.match(line); res.hasMatch())
          {
            specif.handler = res.captured(1);
          }
        }
        else if(line.startsWith("I:"))
        {
          if(auto res = I_rx.match(line); res.hasMatch())
          {
            specif.bus = res.captured(1);
            specif.vendor = res.captured(2);
            specif.product = res.captured(3);
            specif.version = res.captured(4);
          }
        }
      }

      if(!specif.name.isEmpty() && !specif.handler.isEmpty())
      {
        Device::DeviceSettings set;
        set.name = specif.name;
        set.protocol = EvdevProtocolFactory::static_concreteKey();
        set.deviceSpecificSettings = QVariant::fromValue(specif);
        f(set.name, set);
      }
    }
  }
};
QString EvdevProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Evdev");
}

QString EvdevProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl EvdevProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/evdev-device.html");
}

Device::DeviceEnumerators
EvdevProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Devices", new EvdevEnumerator}};
}

Device::DeviceInterface* EvdevProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new EvdevDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& EvdevProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "evdev";
    s.deviceSpecificSettings = QVariant::fromValue(EvdevSpecificSettings{});
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* EvdevProtocolFactory::makeSettingsWidget()
{
  return new EvdevProtocolSettingsWidget;
}

QVariant
EvdevProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<EvdevSpecificSettings>(visitor);
}

void EvdevProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<EvdevSpecificSettings>(data, visitor);
}

bool EvdevProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  auto a_ = a.deviceSpecificSettings.value<EvdevSpecificSettings>();
  // Prevent instantiating a dummy device
  if(a_.name.isEmpty() || a_.handler.isEmpty())
    return false;

  return true;
}
}
#endif
