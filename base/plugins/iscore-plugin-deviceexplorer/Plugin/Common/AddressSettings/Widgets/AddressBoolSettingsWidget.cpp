#include "AddressBoolSettingsWidget.hpp"
#include <QFormLayout>
AddressBoolSettingsWidget::AddressBoolSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_cb = new QComboBox;
    m_cb->addItem(tr("False"), false); // index 0
    m_cb->addItem(tr("True"), true); // index 1
    // The order is important because then
    // setting the index from a bool is a one-liner in setSettings.

    m_layout->insertRow(0, tr("Value"), m_cb);
}

AddressSettings AddressBoolSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    settings.value = m_cb->currentData().value<bool>();

    return settings;
}

void AddressBoolSettingsWidget::setSettings(const AddressSettings& settings)
{
    m_cb->setCurrentIndex(settings.value.value<bool>());
}
