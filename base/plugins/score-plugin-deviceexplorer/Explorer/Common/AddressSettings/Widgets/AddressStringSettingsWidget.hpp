#pragma once

#include "AddressSettingsWidget.hpp"

#include <Device/Address/AddressSettings.hpp>

class QLineEdit;
class QWidget;

namespace State
{
class StringValueSetDialog;
}
namespace Explorer
{
class AddressStringSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressStringSettingsWidget(QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;
  void setSettings(const Device::AddressSettings& settings) override;
  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;

protected:
  QLineEdit* m_valueEdit;
  State::StringValueSetDialog* m_values{};
};
}
