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
    set.value = State::ValueImpl{State::impulse_t{}};
    return set;
}

void AddressImpulseSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
    setCommonSettings(settings);
}
}
