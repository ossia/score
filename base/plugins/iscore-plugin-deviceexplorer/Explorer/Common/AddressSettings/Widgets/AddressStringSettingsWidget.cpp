#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QString>

#include "AddressStringSettingsWidget.hpp"
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/StringValueWidget.hpp>

class QWidget;

namespace Explorer
{
AddressStringSettingsWidget::AddressStringSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
    m_valueEdit = new QLineEdit(this);

    m_values = new State::StringValueSetDialog{this};
    auto pb = new QPushButton{tr("Values"), this};

    m_layout->insertRow(0, makeLabel(tr("Text"), this), m_valueEdit);
    m_layout->insertRow(1, makeLabel(tr("Domain"), this), pb);

    connect(pb, &QPushButton::clicked,
            this, [=] {
        auto vals = m_values->values();
        if(!m_values->exec())
        {
            // Revert to previous values
            m_values->setValues(vals);
        }
    } );
}

Device::AddressSettings AddressStringSettingsWidget::getSettings() const
{
    auto settings = getCommonSettings();
    settings.value.val = m_valueEdit->text();
    settings.domain = ossia::net::domain_base<std::string>{m_values->values()};
    return settings;
}

Device::AddressSettings AddressStringSettingsWidget::getDefaultSettings() const
{
  return {};
}

void
AddressStringSettingsWidget::setSettings(const Device::AddressSettings &settings)
{
    setCommonSettings(settings);
    m_valueEdit->setText(State::convert::value<QString>(settings.value));

    if(auto dom_p = settings.domain.target<ossia::net::domain_base<std::string>>())
        m_values->setValues(dom_p->values);
    else
        m_values->setValues(State::StringValueSetDialog::set_type{});
}
}
