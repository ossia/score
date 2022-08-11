// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressNoneSettingsWidget.hpp"

#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

namespace Explorer
{
AddressNoneSettingsWidget::AddressNoneSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(AddressSettingsWidget::no_widgets_t{}, parent)
{
}

Device::AddressSettings AddressNoneSettingsWidget::getSettings() const
{
  return getCommonSettings();
}

void AddressNoneSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
}

Device::AddressSettings AddressNoneSettingsWidget::getDefaultSettings() const
{
  return {};
}

void AddressNoneSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
}
}
