#include "AddressNoneSettingsWidget.hpp"

AddressNoneSettingsWidget::AddressNoneSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
}

iscore::AddressSettings AddressNoneSettingsWidget::getSettings() const
{
    return getCommonSettings();
}

void AddressNoneSettingsWidget::setSettings(const iscore::AddressSettings& settings)
{
}
