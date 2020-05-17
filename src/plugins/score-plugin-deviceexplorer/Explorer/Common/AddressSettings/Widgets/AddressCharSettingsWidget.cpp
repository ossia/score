// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressCharSettingsWidget.hpp"

#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/CharValueWidget.hpp>

#include <QChar>
#include <QFormLayout>

namespace Explorer
{
AddressCharSettingsWidget::AddressCharSettingsWidget(QWidget* parent)
    : AddressSettingsWidget(parent)
{
  m_valueEdit = new State::CharValueWidget{QChar('-'), this};
  m_domainEdit = new State::CharDomainWidget{this};
  m_layout->insertRow(0, makeLabel(tr("Character"), this), m_valueEdit);
  m_layout->insertRow(1, makeLabel(tr("Domain"), this), m_domainEdit);
}

Device::AddressSettings AddressCharSettingsWidget::getSettings() const
{
  auto settings = getCommonSettings();
  settings.value = m_valueEdit->value();
  settings.domain = m_domainEdit->domain();
  return settings;
}

Device::AddressSettings AddressCharSettingsWidget::getDefaultSettings() const
{
  return {};
}

void AddressCharSettingsWidget::setSettings(const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
  m_valueEdit->setValue(settings.value);
  m_domainEdit->set_domain(settings.domain);
}

void AddressCharSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
  m_domainEdit->setEnabled(b);
}
}
