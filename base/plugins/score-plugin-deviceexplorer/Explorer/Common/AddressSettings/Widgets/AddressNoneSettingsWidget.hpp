#pragma once
#include "AddressSettingsWidget.hpp"

#include <Device/Address/AddressSettings.hpp>

namespace Explorer
{
class AddressNoneSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressNoneSettingsWidget(QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;

  void setSettings(const Device::AddressSettings& settings) override;
  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;
};
}
