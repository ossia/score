#include "AddressNoneSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

class QWidget;



namespace Explorer
{
AddressNoneSettingsWidget::AddressNoneSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(AddressSettingsWidget::no_widgets_t{}, parent)
{
}

Device::AddressSettings AddressNoneSettingsWidget::getSettings() const
{
    auto set = getCommonSettings();
    set.value = State::ValueImpl{State::no_value_t{}};
    return set;
}

void AddressNoneSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
    setCommonSettings(settings);
}



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
