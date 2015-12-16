#include "AddressNoneSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

class QWidget;

AddressImpulseSettingsWidget::AddressImpulseSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
}

iscore::AddressSettings AddressImpulseSettingsWidget::getSettings() const
{
    auto set = getCommonSettings();
    set.value = iscore::ValueImpl{iscore::impulse_t{}};
    return set;
}

void AddressImpulseSettingsWidget::setSettings(const iscore::AddressSettings& settings)
{
    setCommonSettings(settings);
}
