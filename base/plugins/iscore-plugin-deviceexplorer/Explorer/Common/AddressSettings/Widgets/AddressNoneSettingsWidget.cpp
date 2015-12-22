#include "AddressNoneSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

class QWidget;



AddressNoneSettingsWidget::AddressNoneSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(AddressSettingsWidget::no_widgets_t{}, parent)
{
}

iscore::AddressSettings AddressNoneSettingsWidget::getSettings() const
{
    auto set = getCommonSettings();
    set.value = iscore::ValueImpl{iscore::no_value_t{}};
    return set;
}

void AddressNoneSettingsWidget::setSettings(const iscore::AddressSettings& settings)
{
    setCommonSettings(settings);
}



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
