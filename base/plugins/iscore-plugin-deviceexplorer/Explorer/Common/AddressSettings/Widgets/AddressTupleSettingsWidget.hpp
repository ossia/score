#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <QComboBox>
#include <QFormLayout>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/VecWidgets.hpp>

namespace Explorer
{
class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

  Device::AddressSettings getSettings() const override;
  void setSettings(const Device::AddressSettings& settings) override;

  Device::AddressSettings getDefaultSettings() const override;
  void setCanEditProperties(bool b) override;
};
}
