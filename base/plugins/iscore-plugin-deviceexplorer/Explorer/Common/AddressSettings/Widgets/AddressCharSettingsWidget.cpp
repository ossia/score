#include <qchar.h>
#include <qformlayout.h>
#include <qlineedit.h>
#include <qobjectdefs.h>
#include <qstring.h>

#include "AddressCharSettingsWidget.hpp"
#include "Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp"
#include "State/Value.hpp"
#include "State/ValueConversion.hpp"

class QWidget;

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

