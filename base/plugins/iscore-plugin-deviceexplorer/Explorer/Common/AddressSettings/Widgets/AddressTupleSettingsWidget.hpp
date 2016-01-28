#pragma once
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QWidget;

namespace DeviceExplorer
{
class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;

        void setSettings(const Device::AddressSettings& settings) override;
};
}
