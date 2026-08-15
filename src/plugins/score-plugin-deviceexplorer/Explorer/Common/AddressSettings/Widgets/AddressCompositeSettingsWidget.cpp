#include "AddressCompositeSettingsWidget.hpp"

#include <score/tools/Debug.hpp>

#include <ossia/network/value/value.hpp>

#include <QFormLayout>

namespace Explorer
{
namespace
{
ossia::value emptyValueOf(ossia::val_type type)
{
  if(type == ossia::val_type::MAP)
    return ossia::value_map_type{};
  return std::vector<ossia::value>{};
}
}

AddressCompositeSettingsWidget::AddressCompositeSettingsWidget(
    ossia::val_type type, QWidget* parent)
    : AddressSettingsWidget{parent}
    , m_type{type}
{
  Device::AddressSettingsCommon empty;
  empty.value = emptyValueOf(m_type);

  m_value = empty.value;
  m_valueEdit = make_value_widget(empty, this);
  SCORE_ASSERT(m_valueEdit);
  m_layout->insertRow(0, makeLabel(tr("Value"), this), m_valueEdit);
  m_valueEdit->set(m_value);
}

Device::AddressSettings AddressCompositeSettingsWidget::getSettings() const
{
  auto settings = getCommonSettings();

  // Text that reads as another type keeps what was there.
  auto v = m_valueEdit->get();
  settings.value = (v.get_type() == m_type) ? v : m_value;
  return settings;
}

void AddressCompositeSettingsWidget::setSettings(
    const Device::AddressSettings& settings)
{
  setCommonSettings(settings);
  m_value = settings.value.get_type() == m_type ? settings.value : emptyValueOf(m_type);
  m_valueEdit->set(m_value);
}

Device::AddressSettings AddressCompositeSettingsWidget::getDefaultSettings() const
{
  Device::AddressSettings s;
  s.value = emptyValueOf(m_type);
  return s;
}

void AddressCompositeSettingsWidget::setCanEditProperties(bool b)
{
  AddressSettingsWidget::setCanEditProperties(b);
  m_valueEdit->setEnabled(b);
}
}
