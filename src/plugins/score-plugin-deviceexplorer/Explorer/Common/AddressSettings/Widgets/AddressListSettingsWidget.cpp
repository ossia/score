// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressListSettingsWidget.hpp"

namespace Explorer
{

AddressListSettingsWidget::AddressListSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
  // TODO for each value, display the corresopnding settings widget (with
  // min/max/etc...).
  // In order to do this properly,
  // we have to separate the global and per-list settings widgets...
}

Device::AddressSettings AddressListSettingsWidget::getSettings() const
{
  auto settings = getCommonSettings();
  settings.value = std::vector<ossia::value>{};
  return settings;
}

void AddressListSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
}

Device::AddressSettings AddressListSettingsWidget::getDefaultSettings() const
{
  return {};
}

void AddressListSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
}
}
