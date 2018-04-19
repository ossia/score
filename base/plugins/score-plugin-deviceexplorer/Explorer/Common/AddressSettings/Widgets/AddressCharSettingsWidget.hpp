#pragma once

#include "AddressSettingsWidget.hpp"

#include <Device/Address/AddressSettings.hpp>

class QLineEdit;
class QWidget;

namespace State
{
class CharValueWidget;
class CharDomainWidget;
}
namespace Explorer
{
class AddressCharSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressCharSettingsWidget(QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;
  void setSettings(const Device::AddressSettings& settings) override;
  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;

protected:
  State::CharValueWidget* m_valueEdit{};
  State::CharDomainWidget* m_domainEdit{};
};
}
