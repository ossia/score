#include <State/ValueConversion.hpp>
#include <QComboBox>
#include <QFormLayout>

#include <QString>
#include <QVariant>

#include "AddressBoolSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/Value.hpp>

class QWidget;

AddressBoolSettingsWidget::AddressBoolSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_cb = new QComboBox;
    m_cb->addItem(tr("false"), false); // index 0
    m_cb->addItem(tr("true"), true); // index 1
    // The order is important because then
    // setting the index from a bool is a one-liner in setSettings.

    m_layout->insertRow(0, tr("Value"), m_cb);
}

iscore::AddressSettings AddressBoolSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    settings.value.val = m_cb->currentData().value<bool>();

    return settings;
}

void AddressBoolSettingsWidget::setSettings(const iscore::AddressSettings& settings)
{
    setCommonSettings(settings);
    m_cb->setCurrentIndex(iscore::convert::value<bool>(settings.value));
}
