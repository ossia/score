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
    setCommonSettings(settings);
    m_valueEdit->setText(settings.value.val.get<QString>());
}






AddressCharSettingsWidget::AddressCharSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setMaxLength(1);
    m_layout->insertRow(0, tr("Character"), m_valueEdit);
}

iscore::AddressSettings AddressCharSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    auto txt = m_valueEdit->text();
    settings.value.val = txt.length() > 0 ? txt[0] : QChar{};
    return settings;
}

void
AddressCharSettingsWidget::setSettings(const iscore::AddressSettings &settings)
{
    setCommonSettings(settings);
    m_valueEdit->setText(settings.value.val.get<QChar>());
}


