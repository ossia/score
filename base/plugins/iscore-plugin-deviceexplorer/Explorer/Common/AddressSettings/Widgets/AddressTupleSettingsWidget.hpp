#pragma once
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <State/Widgets/Values/VecWidgets.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <QComboBox>
#include <QFormLayout>

namespace Explorer
{
class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;
        void setSettings(const Device::AddressSettings& settings) override;

        Device::AddressSettings getDefaultSettings() const override;
};
}
