#include "AddressTupleSettingsWidget.hpp"

namespace Explorer
{

AddressTupleSettingsWidget::AddressTupleSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
  // TODO for each value, display the corresopnding settings widget (with
  // min/max/etc...).
  // In order to do this properly,
  // we have to separate the global and per-tuple settings widgets...
}

Device::AddressSettings AddressTupleSettingsWidget::getSettings() const
{
  auto settings = getCommonSettings();
  settings.value.val = State::tuple_t{};
  return settings;
}

void AddressTupleSettingsWidget::setSettings(
    const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
}

Device::AddressSettings AddressTupleSettingsWidget::getDefaultSettings() const
{
  return {};
}

void AddressTupleSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
}
}
