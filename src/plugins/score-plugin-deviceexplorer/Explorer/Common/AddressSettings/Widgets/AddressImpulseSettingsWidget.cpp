// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressImpulseSettingsWidget.hpp"

namespace Explorer
{
AddressImpulseSettingsWidget::AddressImpulseSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
}

Device::AddressSettings AddressImpulseSettingsWidget::getSettings() const
{
  auto set = getCommonSettings();
  set.value = ossia::value{State::impulse{}};
  return set;
}

void AddressImpulseSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
}

Device::AddressSettings AddressImpulseSettingsWidget::getDefaultSettings() const
{
  return {};
}

void AddressImpulseSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
}
}
