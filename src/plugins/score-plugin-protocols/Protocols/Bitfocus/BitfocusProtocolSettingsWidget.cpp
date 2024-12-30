// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusProtocolSettingsWidget.hpp"

#include "BitfocusContext.hpp"
#include "BitfocusProtocolFactory.hpp"
#include "BitfocusSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/NetworkWidgets/TCPWidget.hpp>
#include <Protocols/NetworkWidgets/WebsocketClientWidget.hpp>
#include <Protocols/RateWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QDir>
#include <QStackedLayout>
#include <QVariant>

#include <wobjectimpl.h>

namespace Protocols
{

BitfocusProtocolSettingsWidget::BitfocusProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
}

Device::DeviceSettings BitfocusProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = BitfocusProtocolFactory::static_concreteKey();

  BitfocusSpecificSettings osc = m_settings;
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

void BitfocusProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if(settings.deviceSpecificSettings.canConvert<BitfocusSpecificSettings>())
  {
    auto set = settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
    if(!set.path.isEmpty() && QDir{set.path}.exists())
    {
      auto module = new bitfocus::module_handler{set.path};
      connect(module, &bitfocus::module_handler::configurationParsed, this, [] {

      });
    }

    m_settings = set;
  }
}
}
