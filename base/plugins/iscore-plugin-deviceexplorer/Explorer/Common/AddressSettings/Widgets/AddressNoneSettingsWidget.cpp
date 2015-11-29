#include "AddressNoneSettingsWidget.hpp"
#include "Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp"

class QWidget;

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
    setCommonSettings(settings);
}
