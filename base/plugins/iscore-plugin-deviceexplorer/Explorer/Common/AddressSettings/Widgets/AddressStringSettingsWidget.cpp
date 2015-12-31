#include <QFormLayout>
#include <QLineEdit>

#include <QString>

#include "AddressStringSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

class QWidget;

AddressStringSettingsWidget::AddressStringSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_valueEdit = new QLineEdit(this);
    m_layout->insertRow(0, tr("Text"), m_valueEdit);
}

Device::AddressSettings AddressStringSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    settings.value.val = m_valueEdit->text();
    return settings;
}

void
AddressStringSettingsWidget::setSettings(const Device::AddressSettings &settings)
{
    setCommonSettings(settings);
    m_valueEdit->setText(State::convert::value<QString>(settings.value));
}
