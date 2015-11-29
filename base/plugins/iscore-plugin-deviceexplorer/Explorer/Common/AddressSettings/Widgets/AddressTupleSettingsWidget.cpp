#include "AddressTupleSettingsWidget.hpp"
#include "Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp"

class QWidget;

AddressTupleSettingsWidget::AddressTupleSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    // TODO for each value, display the corresopnding settings widget (with min/max/etc...).
    // In order to do this properly,
    // we have to separate the global and per-tuple settings widgets...
}

iscore::AddressSettings AddressTupleSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    return settings;
}

void AddressTupleSettingsWidget::setSettings(const iscore::AddressSettings& settings)
{
    setCommonSettings(settings);
}
