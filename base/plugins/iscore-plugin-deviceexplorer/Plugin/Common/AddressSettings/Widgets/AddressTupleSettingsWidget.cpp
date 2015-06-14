#include "AddressTupleSettingsWidget.hpp"
AddressTupleSettingsWidget::AddressTupleSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{

}

AddressSettings AddressTupleSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    return settings;
}

void AddressTupleSettingsWidget::setSettings(const AddressSettings& settings)
{

}
