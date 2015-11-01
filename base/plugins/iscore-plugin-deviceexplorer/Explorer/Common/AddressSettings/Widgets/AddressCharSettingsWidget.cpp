#include "AddressCharSettingsWidget.hpp"
#include <QLineEdit>
#include <QFormLayout>

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
    m_valueEdit->setText(iscore::convert::value<QChar>(settings.value));
}

