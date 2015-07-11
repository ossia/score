#include "AddressNoneSettingsWidget.hpp"

AddressNoneSettingsWidget::AddressNoneSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
}

AddressSettings AddressNoneSettingsWidget::getSettings() const
{
    return getCommonSettings();
}

void AddressNoneSettingsWidget::setSettings(const AddressSettings& settings)
{
}
