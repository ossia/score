#include <QChar>
#include <QFormLayout>
#include <QLineEdit>

#include <QString>

#include "AddressCharSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

class QWidget;

namespace DeviceExplorer
{
AddressCharSettingsWidget::AddressCharSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setMaxLength(1);
    m_layout->insertRow(0, tr("Character"), m_valueEdit);
}

Device::AddressSettings AddressCharSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    auto txt = m_valueEdit->text();
    settings.value.val = txt.length() > 0 ? txt[0] : QChar{};
    return settings;
}

void
AddressCharSettingsWidget::setSettings(const Device::AddressSettings &settings)
{
    setCommonSettings(settings);
    m_valueEdit->setText(State::convert::value<QChar>(settings.value));
}
}
