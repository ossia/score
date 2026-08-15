#pragma once
#include <Device/Address/AddressSettings.hpp>

#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Explorer/Explorer/ValueEditors.hpp>

#include <ossia/network/common/parameter_properties.hpp>

namespace Explorer
{
//! Lists and maps in the add / edit address dialog: they have no per-component
//! form, only the textual one the value parser reads back.
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressCompositeSettingsWidget final
    : public AddressSettingsWidget
{
public:
  explicit AddressCompositeSettingsWidget(
      ossia::val_type type, QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;
  void setSettings(const Device::AddressSettings& settings) override;

  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;

private:
  ossia::val_type m_type{};
  ossia::value m_value;
  AddressValueWidget* m_valueEdit{};
};
}
