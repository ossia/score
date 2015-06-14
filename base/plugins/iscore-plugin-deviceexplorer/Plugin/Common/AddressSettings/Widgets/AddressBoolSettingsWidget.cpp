#include "AddressBoolSettingsWidget.hpp"
AddressBoolSettingsWidget::AddressBoolSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    // TODO only value (true / false)

}

AddressSettings AddressBoolSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    return settings;
}

void AddressBoolSettingsWidget::setSettings(const AddressSettings& settings)
{

}
