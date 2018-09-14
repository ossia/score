#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/VecWidgets.hpp>

#include <QComboBox>
#include <QFormLayout>

namespace Explorer
{
class AddressListSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressListSettingsWidget(QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;
  void setSettings(const Device::AddressSettings& settings) override;

  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;
};
}
