#include "AddressStringSettingsWidget.hpp"

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>

AddressStringSettingsWidget::AddressStringSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_valueEdit = new QLineEdit(this);
    m_layout->insertRow(0, tr("Text"), m_valueEdit);
}

iscore::AddressSettings AddressStringSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    settings.value.val = m_valueEdit->text();
    return settings;
}

void
AddressStringSettingsWidget::setSettings(const iscore::AddressSettings &settings)
{
    m_valueEdit->setText(settings.value.val.value<QString>());
}


